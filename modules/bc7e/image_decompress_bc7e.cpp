/*************************************************************************/
/*  image_decompress_bc7e.cpp                                            */
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
#include "modules/bc7e/image_decompress_bc7e.h"
#include "thirdparty/bc7e/bc7decomp.h"

#include "core/io/image.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/string/print_string.h"

#include <limits>
#include <vector>

struct bc7_block {
	uint64_t m_vals[2];
};

void image_decompress_bc7e(Image *p_image) {
	Image::Format input_format = p_image->get_format();
	if (input_format != Image::FORMAT_RGB8 && input_format != Image::FORMAT_RGBA8) {
		return;
	}

	Image::Format target_format;
	bool is_signed = false;

	switch (input_format) {
		case Image::FORMAT_BPTC_RGBA:
			target_format = Image::FORMAT_RGBA8;
			break;
		default:
			return; // Invalid input format
	};

	int w = p_image->get_width();
	int h = p_image->get_height();

	const uint8_t *rb = p_image->get_data().ptr();

	Vector<uint8_t> data;
	int target_size = Image::get_image_data_size(w, h, target_format, p_image->has_mipmaps());
	int mm_count = p_image->get_mipmap_count();
	data.resize(target_size);

	uint8_t *wb = data.ptrw();

	int bytes_per_pixel = 4;

	int dst_ofs = 0;

	if (w % 4 != 0 && h % 4 != 0) {
		return;
	}

	for (int i = 0; i <= mm_count; i++) {
		int src_ofs = p_image->get_mipmap_offset(i);

		const uint8_t *in_bytes = &rb[src_ofs];
		uint32_t flag = DETEX_DECOMPRESS_FLAG_NON_OPAQUE_ONLY;

		const uint32_t blocks_x = w / 4;
		const uint32_t blocks_y = h / 4;

		for (uint32_t by = 0; by < blocks_y; by++) {
			for (uint32_t bx = 0; bx < blocks_x; bx++) {
				uint8_t *out_bytes = &wb[dst_ofs];
				const uint8_t *pBlock = &in_bytes[bx + by * blocks_x];
				detexDecompressBlockBPTC((const uint8_t *)pBlock, UINT32_MAX, 0, (uint8_t *)out_bytes);
				dst_ofs += 4 * bytes_per_pixel;
			}
		}
	}

	p_image->create(p_image->get_width(), p_image->get_height(), p_image->has_mipmaps(), target_format, data);
}
