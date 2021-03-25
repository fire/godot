#include "image_etc.h"
#include "core/io/image.h"
#include "core/os/copymem.h"
#include "core/os/os.h"
#include "core/string/print_string.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <future>
#include <limits>
#include <memory>

#ifdef _MSC_VER
#include "getopt/getopt.h"
#else
#include <getopt.h>
#include <unistd.h>
#endif

#include "etcpak_wrap.h"

void _register_etc_compress_func() {
	Image::_image_compress_etc1_func = _compress_etc1;
	Image::_image_compress_etc2_func = _compress_etc2;
	Image::_image_compress_bc_func = _compress_bc;
}
static void _compress_etc2(Image *p_img, float p_lossy_quality, Image::UsedChannels p_source) {
	_compress_etc(p_img, p_lossy_quality, false, p_source);
}
static void _compress_etc1(Image *p_img, float p_lossy_quality) {
	_compress_etc(p_img, p_lossy_quality, true, Image::USED_CHANNELS_RGB);
}
static void _compress_etc(Image *p_img, float p_lossy_quality, bool force_etc1_format, Image::UsedChannels p_channels) {
	Image::Format img_format = p_img->get_format();

	if (img_format >= Image::FORMAT_DXT1) {
		return; //do not compress, already compressed
	}

	if (img_format > Image::FORMAT_RGBA8) {
		// TODO: we should be able to handle FORMAT_RGBA4444 and FORMAT_RGBA5551 eventually
		return;
	}

	uint32_t imgw = p_img->get_width(), imgh = p_img->get_height();

	Image::Format etc_format = force_etc1_format ? Image::FORMAT_ETC : _get_etc2_mode(p_channels);

	Ref<Image> img = p_img->duplicate();

	if (img->has_mipmaps()) {
		if (next_power_of_2(imgw) != imgw || next_power_of_2(imgh) != imgh) {
			img->resize_to_po2();
			imgw = img->get_width();
			imgh = img->get_height();
		}
	} else {
		if (imgw % 4 != 0 || imgh % 4 != 0) {
			if (imgw % 4) {
				imgw += 4 - imgw % 4;
			}
			if (imgh % 4) {
				imgh += 4 - imgh % 4;
			}

			img->resize(imgw, imgh);
		}
	}

	print_verbose("Encoding format: " + Image::get_format_name(etc_format));
	uint64_t t = OS::get_singleton()->get_ticks_msec();
	if (etc_format == Image::FORMAT_ETC || force_etc1_format) {
		etc_format = Image::FORMAT_ETC;
	} else if (etc_format == Image::FORMAT_ETC2_RGB8) {
		etc_format = Image::FORMAT_ETC2_RGB8;
	} else if (etc_format == Image::FORMAT_ETC2_RGBA8) {
		etc_format = Image::FORMAT_ETC2_RGBA8;
	} else if (etc_format == Image::FORMAT_ETC2_R11) {
		etc_format = Image::FORMAT_ETC2_RGB8;
		img->convert(Image::FORMAT_RGB8);
	} else if (etc_format == Image::FORMAT_ETC2_RG11) {
		etc_format = Image::FORMAT_ETC2_RGB8;
		img->convert(Image::FORMAT_RGB8);
	} else {
		etc_format = Image::FORMAT_ETC2_RGBA8;
	}
	unsigned int target_size = Image::get_image_data_size(imgw, imgh, etc_format, p_img->has_mipmaps());
	Vector<uint8_t> dst_data;
	dst_data.resize(target_size);
	const bool dither = false;
	const bool mipmap = true;
	Vector<uint32_t> tex;
	tex.resize(imgh * imgw);
	size_t count = 0;
	for (size_t y = 0; y < imgh; y++) {
		for (size_t x = 0; x < imgw; x++) {
			Color c = img->get_pixel(x, y);
			tex.ptrw()[count] = c.to_rgba32();
			count++;
		}
	}
	bool etc2 = etc_format == Image::FORMAT_ETC2_RGBA8 ? true : false;
	do_stuff(imgw, imgh, dither, etc2, img->get_size().x, img->get_size().y, mipmap, tex.to_byte_array().ptr(), target_size, dst_data.ptrw());
	p_img->create(imgw, imgh, p_img->has_mipmaps(), etc_format, dst_data);
	print_verbose("ETCPAK encode took " + rtos(OS::get_singleton()->get_ticks_msec() - t));
}

static void _compress_bc(Image *p_img, float p_lossy_quality, Image::UsedChannels p_channels) {
	Image::Format img_format = p_img->get_format();

	if (img_format >= Image::FORMAT_DXT1) {
		return; //do not compress, already compressed
	}

	if (img_format > Image::FORMAT_RGBA8) {
		// TODO: we should be able to handle FORMAT_RGBA4444 and FORMAT_RGBA5551 eventually
		return;
	}

	uint32_t imgw = p_img->get_width(), imgh = p_img->get_height();

	Image::Format format = Image::FORMAT_DXT5;

	Ref<Image> img = p_img->duplicate();

	if (img->has_mipmaps()) {
		if (next_power_of_2(imgw) != imgw || next_power_of_2(imgh) != imgh) {
			img->resize_to_po2();
			imgw = img->get_width();
			imgh = img->get_height();
		}
	} else {
		if (imgw % 4 != 0 || imgh % 4 != 0) {
			if (imgw % 4) {
				imgw += 4 - imgw % 4;
			}
			if (imgh % 4) {
				imgh += 4 - imgh % 4;
			}

			img->resize(imgw, imgh);
		}
	}

	print_verbose("Encoding format: " + Image::get_format_name(format));
	const bool mipmap = true;
	uint64_t t = OS::get_singleton()->get_ticks_msec();
		unsigned int target_size = Image::get_image_data_size(imgw, imgh, format, mipmap);
	Vector<uint8_t> dst_data;
	dst_data.resize(target_size);
	Vector<uint32_t> tex;
	tex.resize(imgh * imgw);
	size_t count = 0;
	for (size_t y = 0; y < imgh; y++) {
		for (size_t x = 0; x < imgw; x++) {
			Color c = img->get_pixel(x, y);
			tex.ptrw()[count] = c.to_abgr32();
			count++;
		}
	}
	do_stuff_bc(imgw, imgh, img->get_size().x, img->get_size().y, mipmap, tex.to_byte_array().ptr(), target_size, dst_data.ptrw());
	p_img->create(imgw, imgh, mipmap, format, dst_data);
	print_verbose("ETCPAK encode took " + rtos(OS::get_singleton()->get_ticks_msec() - t));
}

static Image::Format _get_etc2_mode(Image::UsedChannels format) {
	switch (format) {
		case Image::USED_CHANNELS_R:
			return Image::FORMAT_ETC2_R11;

		case Image::USED_CHANNELS_RG:
			return Image::FORMAT_ETC2_RG11;

		case Image::USED_CHANNELS_RGB:
			return Image::FORMAT_ETC2_RGB8;

		case Image::USED_CHANNELS_RGBA:
			return Image::FORMAT_ETC2_RGBA8;

		// TODO: would be nice if we could use FORMAT_ETC2_RGB8A1 for FORMAT_RGBA5551
		default:
			// TODO: Kept for compatibility, but should be investigated whether it's correct or if it should error out
			return Image::FORMAT_ETC2_RGBA8;
	}
}