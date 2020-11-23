/*************************************************************************/
/*  image_betsy.cp                                                       */
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

#include "core/io/image.h"
#include "core/os/copymem.h"
#include "core/os/os.h"
#include "core/string/print_string.h"

//#include "betsy/EncoderETC1.h"
//#include "betsy/EncoderETC2.h"
#include "betsy/EncoderBC1.h"

namespace betsy {
void initBetsyPlatform() {}
void shutdownBetsyPlatform() {}
void pollPlatformWindow() {}
} // namespace betsy

//void _compress_etc(Image *p_img, float p_lossy_quality, bool force_etc1_format, Image::UsedChannels p_channels) {
//	Image::Format img_format = p_img->get_format();
//
//	if (img_format >= Image::FORMAT_DXT1) {
//		return; //do not compress, already compressed
//	}
//
//	if (img_format > Image::FORMAT_RGBA8) {
//		// TODO: we should be able to handle FORMAT_RGBA4444 and FORMAT_RGBA5551 eventually
//		return;
//	}	
//	uint64_t t = OS::get_singleton()->get_ticks_msec();
//
//	betsy::EncoderETC2 encoder;
//	bool dither = false;
//	bool usingRenderDoc = false;
//	bool etc2_rgba = true;
//	encoder.initResources(p_img, etc2_rgba, dither);
//	size_t repeat = usingRenderDoc ? 2u : 1u;
//	while (repeat--) {
//		encoder.execute00();
//		encoder.execute01(betsy::EncoderETC1::Etc1Quality::cHighQuality);
//		encoder.execute02();
//		if (usingRenderDoc) {
//			encoder.execute03(); // Not needed in offline mode
//		}
//		betsy::pollPlatformWindow();
//	}
//	print_verbose("ETC: Time encoding: " + rtos(OS::get_singleton()->get_ticks_msec() - t));
//	encoder.downloadTo(p_img);
//	encoder.deinitResources();
//}
//
//void _compress_etc1(Image *p_img, float p_lossy_quality) {
//	_compress_etc(p_img, p_lossy_quality, true, Image::USED_CHANNELS_RGB);
//}
//
//void _compress_etc2(Image *p_img, float p_lossy_quality, Image::UsedChannels p_channels) {
//	_compress_etc(p_img, p_lossy_quality, false, p_channels);
//}

void _compress_bc(Image *p_img, float p_lossy_quality, Image::UsedChannels p_channels) {
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

