/*************************************************************************/
/*  image_etc.cpp                                                        */
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

#include "image_betsy.h"

#include "core/image.h"
#include "core/os/copymem.h"
#include "core/os/os.h"
#include "core/print_string.h"

#include "betsy/EncoderETC1.h"
#include "betsy/EncoderETC2.h"
#include "betsy/EncoderBC1.h"

namespace betsy {
extern void initBetsyPlatform();
extern void shutdownBetsyPlatform();
extern void pollPlatformWindow();
} // namespace betsy

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

static void _compress_etc(Image *p_img, float p_lossy_quality, bool force_etc1_format, Image::UsedChannels p_channels) {
	Image::Format img_format = p_img->get_format();

	if (img_format >= Image::FORMAT_DXT1) {
		return; //do not compress, already compressed
	}

	if (img_format > Image::FORMAT_RGBA8) {
		// TODO: we should be able to handle FORMAT_RGBA4444 and FORMAT_RGBA5551 eventually
		return;
	}	
	uint64_t t = OS::get_singleton()->get_ticks_msec();

	betsy::EncoderETC2 encoder;
	bool dither = false;
	bool usingRenderDoc = false;
	bool etc2_rgba = true;
	encoder.initResources(p_img, etc2_rgba, dither);
	size_t repeat = usingRenderDoc ? 2u : 1u;
	while (repeat--) {
		encoder.execute00();
		encoder.execute01(betsy::EncoderETC1::Etc1Quality::cHighQuality);
		encoder.execute02();
		if (usingRenderDoc) {
			encoder.execute03(); // Not needed in offline mode
		}
		betsy::pollPlatformWindow();
	}
	print_verbose("ETC: Time encoding: " + rtos(OS::get_singleton()->get_ticks_msec() - t));
	encoder.downloadTo(p_img);
	encoder.deinitResources();
}

static void _compress_etc1(Image *p_img, float p_lossy_quality) {
	_compress_etc(p_img, p_lossy_quality, true, Image::USED_CHANNELS_RGB);
}

static void _compress_etc2(Image *p_img, float p_lossy_quality, Image::UsedChannels p_channels) {
	_compress_etc(p_img, p_lossy_quality, false, p_channels);
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
	uint64_t t = OS::get_singleton()->get_ticks_msec();
	betsy::EncoderBC1 encoder;
	bool dither = false;
	bool usingRenderDoc = false;
	bool rgba = true;
	encoder.initResources(p_img, rgba, dither);
	size_t repeat = usingRenderDoc ? 2u : 1u;
	while (repeat--) {
		encoder.execute01();
		encoder.execute02();
		if (usingRenderDoc) {
			encoder.execute03(); // Not needed in offline mode
		}
		betsy::pollPlatformWindow();
	}
	print_verbose("ETC: Time encoding: " + rtos(OS::get_singleton()->get_ticks_msec() - t));
	encoder.downloadTo(p_img);
	encoder.deinitResources();
}

void _register_etc_compress_func() {
	Image::_image_compress_etc1_func = _compress_etc1;
	Image::_image_compress_etc2_func = _compress_etc2;
	Image::_image_compress_bc_func = _compress_bc;
}
