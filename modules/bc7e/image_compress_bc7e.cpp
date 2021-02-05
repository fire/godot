/*************************************************************************/
/*  image_compress_bc7e.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "image_compress_bc7e.h"
#include "bc7e.h"

#include "core/io/image.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/threaded_array_processor.h"
#include "core/string/print_string.h"

// Want to expose this.
class ImageMetrics {
public:
	double m_max, m_mean, m_mean_squared, m_root_mean_squared, m_peak_snr;

	ImageMetrics() {
		clear();
	}

	void clear() {
		memset(this, 0, sizeof(*this));
	}

	void compute(const image_u8 &a, const image_u8 &b, uint32_t first_channel, uint32_t num_channels) {
		const bool average_component_error = true;

		const uint32_t width = std::min(a.width(), b.width());
		const uint32_t height = std::min(a.height(), b.height());

		ERR_FAIL_COND(!((first_channel < 4U) && (first_channel + num_channels <= 4U)));

		// Histogram approach originally due to Charles Bloom.
		double hist[256];
		memset(hist, 0, sizeof(hist));

		for (uint32_t y = 0; y < height; y++) {
			for (uint32_t x = 0; x < width; x++) {
				const color_quad_u8 &ca = a(x, y);
				const color_quad_u8 &cb = b(x, y);

				if (!num_channels)
					hist[iabs(ca.get_luma() - cb.get_luma())]++;
				else {
					for (uint32_t c = 0; c < num_channels; c++)
						hist[iabs(ca[first_channel + c] - cb[first_channel + c])]++;
				}
			}
		}

		m_max = 0;
		double sum = 0.0f, sum2 = 0.0f;
		for (uint32_t i = 0; i < 256; i++) {
			if (!hist[i])
				continue;

			m_max = std::max<double>(m_max, i);

			double x = i * hist[i];

			sum += x;
			sum2 += i * x;
		}

		// See http://richg42.blogspot.com/2016/09/how-to-compute-psnr-from-old-berkeley.html
		double total_values = width * height;

		if (average_component_error)
			total_values *= clamp<uint32_t>(num_channels, 1, 4);

		m_mean = clamp<double>(sum / total_values, 0.0f, 255.0f);
		m_mean_squared = clamp<double>(sum2 / total_values, 0.0f, 255.0f * 255.0f);

		m_root_mean_squared = sqrt(m_mean_squared);

		if (!m_root_mean_squared)
			m_peak_snr = 1e+10f;
		else
			m_peak_snr = clamp<double>(log10(255.0f / m_root_mean_squared) * 20.0f, 0.0f, 500.0f);
	}
};

void image_compress_bc7e(Image *p_image, float p_lossy_quality, Image::UsedChannels p_channels) {
	struct bc7_block {
		uint64_t m_vals[2];
	};
	struct BC7EData {
		int blocks_x = 0;
		image_u8 *mip_source_image = nullptr;
		int by = 0;
		Vector<bc7_block> *packed_image = nullptr;
		ispc::bc7e_compress_block_params packed_params;
		BC7EData() {}
		BC7EData(int p_blocks_x, image_u8 *p_mip_source_image, int p_by, Vector<bc7_block> *p_packed_image, ispc::bc7e_compress_block_params p_pack_params) :
				blocks_x(p_blocks_x), mip_source_image(p_mip_source_image), by(p_by), packed_image(p_packed_image), packed_params(p_pack_params) {
		}
	};
	struct CompressBlocks {
		void compress_blocks(uint32_t index, BC7EData *bc7e_data) {
			const int N = 64;

			BC7EData *data = bc7e_data + index;
			for (uint32_t bx = 0; bx < data->blocks_x; bx += N) {
				const uint32_t num_blocks_to_process = std::min<uint32_t>(data->blocks_x - bx, N);

				color_quad_u8 pixels[16 * N];

				// Extract num_blocks_to_process 4x4 pixel blocks from the source image and put them into the pixels[] array.
				for (uint32_t b = 0; b < num_blocks_to_process; b++) {
					data->mip_source_image->get_block(bx + b, data->by, 4, 4, pixels + (b * 16));
				}

				// Compress the blocks to BC7.
				// Note: If you've used Intel's ispc_texcomp, the input pixels are different. BC7E requires a pointer to an array of 16 pixels for each block.
				bc7_block *pBlock = &data->packed_image->ptrw()[bx + data->by * data->blocks_x];
				ispc::bc7e_compress_blocks(num_blocks_to_process, reinterpret_cast<uint64_t *>(pBlock), reinterpret_cast<const uint32_t *>(pixels), &data->packed_params);
			}
		}
	};
	Image::Format input_format = p_image->get_format();
	if (input_format >= Image::FORMAT_BPTC_RGBA) {
		return; //do not compress, already compressed
	}
	if (input_format != Image::FORMAT_RGB8 && input_format != Image::FORMAT_RGBA8) {
		return;
	}

	clock_t start_t = clock();
	Image::Format target_format = Image::FORMAT_BPTC_RGBA;
	int img_w = p_image->get_width();
	int img_h = p_image->get_height();
	ispc::bc7e_compress_block_init();

	bool perceptual = true;
	int uber_level = 3;
	if (Math::is_equal_approx(p_lossy_quality, 1.0f)) {
		uber_level = 6;
	} else if (p_lossy_quality > 0.85) {
		uber_level = 5;
	} else if (p_lossy_quality > 0.75) {
		uber_level = 4;
	} else if (p_lossy_quality > 0.55) {
		uber_level = 3;
	} else if (p_lossy_quality > 0.35) {
		uber_level = 2;
	} else if (p_lossy_quality > 0.15) {
		uber_level = 1;
	}

	ispc::bc7e_compress_block_params pack_params;
	memset(&pack_params, 0, sizeof(pack_params));
	switch (uber_level) {
		case 0:
			ispc::bc7e_compress_block_params_init_ultrafast(&pack_params, perceptual);
			break;
		case 1:
			ispc::bc7e_compress_block_params_init_veryfast(&pack_params, perceptual);
			break;
		case 2:
			ispc::bc7e_compress_block_params_init_fast(&pack_params, perceptual);
			break;
		case 3:
			ispc::bc7e_compress_block_params_init_basic(&pack_params, perceptual);
			break;
		case 4:
			ispc::bc7e_compress_block_params_init_slow(&pack_params, perceptual);
			break;
		case 5:
			ispc::bc7e_compress_block_params_init_veryslow(&pack_params, perceptual);
			break;
		case 6:
		default:
			ispc::bc7e_compress_block_params_init_slowest(&pack_params, perceptual);
			break;
	}
	Ref<Image> new_img;
	new_img.instance();
	new_img->create(p_image->get_width(), p_image->get_height(), p_image->has_mipmaps(), target_format);

	Vector<uint8_t> data = new_img->get_data();

	uint8_t *wr = data.ptrw();
	const uint8_t *r = p_image->get_data().ptr();

	Ref<Image> image = p_image->duplicate();
	int mmc = 1 + (new_img->has_mipmaps() ? Image::get_image_required_mipmaps(new_img->get_width(), new_img->get_height(), target_format) : 0);
	for (int i = 0; i < mmc; i++) {
		int ofs, size, mip_w, mip_h;
		new_img->get_mipmap_offset_size_and_dimensions(i, ofs, size, mip_w, mip_h);
		mip_w = (mip_w + 3) & ~3;
		mip_h = (mip_h + 3) & ~3;
		image->resize(mip_w, mip_h);
		const uint32_t blocks_y = mip_h / 4;
		const uint32_t blocks_x = mip_w / 4;
		Vector<bc7_block> packed_image;
		packed_image.resize(blocks_x * blocks_y);
		image_u8 mip_source_image(mip_w, mip_h);
		PackedByteArray image_data = image->get_data();
		for (uint32_t y = 0; y < mip_h; y++) {
			for (uint32_t x = 0; x < mip_w; x++) {
				Color c = image->get_pixel(x, y);
				uint8_t r = c.get_r8();
				uint8_t g = c.get_g8();
				uint8_t b = c.get_b8();
				uint8_t a = c.get_a8();
				mip_source_image(x, y).set(r, g, b, a);
			}
		}
		Vector<BC7EData> bc7e_arr;
		bc7e_arr.resize(blocks_y);
		for (int32_t by = 0; by < blocks_y; by++) {
			BC7EData bc7e_data(blocks_x, &mip_source_image, by, &packed_image, pack_params);
			bc7e_arr.write[by] = bc7e_data;
		}
		CompressBlocks bc7e_compress_blocks;
		thread_process_array(blocks_y, &bc7e_compress_blocks, &CompressBlocks::compress_blocks, bc7e_arr.ptrw());
		int target_size = packed_image.size() * sizeof(bc7_block);
		ERR_FAIL_COND(target_size != size);
		copymem(&wr[ofs], packed_image.ptr(), size);

		if (false) {
			image_u8 unpacked_image(mip_w, mip_h);
			for (uint32_t by = 0; by < blocks_y; by++) {
				for (uint32_t bx = 0; bx < blocks_x; bx++) {
					bc7_block *pBlock = &packed_image.ptrw()[bx + by * blocks_x];

					color_quad_u8 unpacked_pixels[16];
					detexDecompressBlockBPTC((const uint8_t *)pBlock, UINT32_MAX, 0, (uint8_t *)unpacked_pixels);

					unpacked_image.set_block(bx, by, 4, 4, unpacked_pixels);
				}
			}
			// TODO 2021-02-01 fire expose as debug
			ImageMetrics y_metrics;
			y_metrics.compute(mip_source_image, unpacked_image, 0, 0);
			print_line(vformat("Luma\tMax error: %.0f RMSE: %.2f PSNR %.2f dB", y_metrics.m_max, y_metrics.m_root_mean_squared, y_metrics.m_peak_snr));

			ImageMetrics rgb_metrics;
			rgb_metrics.compute(mip_source_image, unpacked_image, 0, 3);
			print_line(vformat("RGB\tMax error: %.0f RMSE: %.2f PSNR %.2f dB", rgb_metrics.m_max, rgb_metrics.m_root_mean_squared, rgb_metrics.m_peak_snr));

			ImageMetrics rgba_metrics;
			rgba_metrics.compute(mip_source_image, unpacked_image, 0, 4);
			print_line(vformat("RGBA\tMax error: %.0f RMSE: %.2f PSNR %.2f dB", rgba_metrics.m_max, rgba_metrics.m_root_mean_squared, rgba_metrics.m_peak_snr));

			ImageMetrics a_metrics;
			a_metrics.compute(mip_source_image, unpacked_image, 3, 1);
			print_line(vformat("Alpha\tMax error: %.0f RMSE: %.2f PSNR %.2f dB", a_metrics.m_max, a_metrics.m_root_mean_squared, a_metrics.m_peak_snr));
		}
	}

	p_image->create(new_img->get_width(), new_img->get_height(), new_img->has_mipmaps(), new_img->get_format(), data);
	clock_t end_t = clock();
	print_line(vformat("Total time: %.2f secs", (double)(end_t - start_t) / CLOCKS_PER_SEC));
}
