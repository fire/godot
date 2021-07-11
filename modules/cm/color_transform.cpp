/*************************************************************************/
/*  color_transform.cpp                                                  */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "color_transform.h"
#include "core/error/error_macros.h"
#include "core/io/image.h"
#include "core/variant/variant.h"
#include "pngconf.h"
#include "servers/rendering_server.h"
#include <lcms2.h>
#include <stdint.h>

bool ColorTransform::is_valid() {
	return src_profile.is_valid() && src_profile->is_valid() && dst_profile.is_valid() && dst_profile->is_valid();
}

void ColorTransform::set_src_profile(Ref<ColorProfile> p_profile) {
	if (p_profile.is_null()) {
		ERR_PRINT("Source profile is null.");
		return;
	}
	src_profile = p_profile;
}

void ColorTransform::set_dst_profile(Ref<ColorProfile> p_profile) {
	if (p_profile.is_null()) {
		ERR_PRINT("Destination profile is null.");
		return;
	}
	dst_profile = p_profile;
}

void ColorTransform::set_intent(Intent p_intent) {
	intent = (cmsUInt32Number)p_intent;
}

void ColorTransform::set_black_point_compensation(bool p_black_point_compensation) {
	use_bpc = p_black_point_compensation;
}

RID ColorTransform::get_color_correction() {
	if (!is_valid()) {
		ERR_PRINT("Transform is not valid.");
		return RID();
	}
	Ref<Image> out_lut;
	out_lut.instantiate();
	int32_t texture_dim = 4096;
	int32_t tile_dim = 256;
	out_lut->create(texture_dim, texture_dim, false, Image::FORMAT_RGB8);
	int32_t root = Math::sqrt((float)tile_dim);
	{
		// hard-coded 256x256x256 size for now
		// Fill image with identity data in source space
		for (int y = 0; y < texture_dim; y++) {
			for (int x = 0; x < texture_dim; x++) {
				Color c;
				c.r = x % tile_dim / float(tile_dim - 1);
				c.g = y % tile_dim / float(tile_dim - 1);
				c.b = ((float(y) / tile_dim * root) + (x % tile_dim / root)) / float(tile_dim - 1);
				out_lut->set_pixel(x, y, c);
			}
		}
	}
	{
		// Use LCMS to transform data
		cmsHPROFILE src = src_profile->get_profile_handle().profile; // handles owned by ColorProfile, don't free
		cmsHPROFILE dst = dst_profile->get_profile_handle().profile;
		if (src == nullptr || dst == nullptr) {
			ERR_PRINT("Transform has invalid profiles. This should have been checked earlier.");
			return RID();
		}
		cmsUInt32Number flags = use_bpc ? cmsFLAGS_BLACKPOINTCOMPENSATION : 0;
		// Half allows hdr icc profiless
		// Identity check
		cmsHTRANSFORM transform = cmsCreateTransform(src, TYPE_RGB_8, dst, TYPE_RGB_8, intent, flags);
		ERR_FAIL_COND_V_MSG(!transform, RID(), "Failed to create lcms transform.");
		PackedByteArray out_array;
		out_array.resize(out_lut->get_mipmap_byte_size(0));
		cmsDoTransform(transform, out_lut->get_data().to_byte_array().ptr(), out_array.ptrw(), texture_dim * texture_dim); // cmsDoTransform wants number of pixels
		cmsDeleteTransform(transform); // we don't need it after this one use
		out_lut->create(texture_dim, texture_dim, false, Image::FORMAT_RGB8, out_array);
	}
	if (out_lut.is_null()) {
		ERR_PRINT("Failed to create LUT texture.");
		return RID();
	}
	out_lut->set_name("LUT Texture");
	RID tex_3d;
	if (false) {
		int h_slices = 16;
		int v_slices = 16;

		Vector<Ref<Image>> slices;
		int slice_w = out_lut->get_width() / h_slices;
		int slice_h = out_lut->get_height() / v_slices;

		for (int i = 0; i < v_slices; i++) {
			for (int j = 0; j < h_slices; j++) {
				int x = slice_w * j;
				int y = slice_h * i;
				Ref<Image> slice = out_lut->get_rect(Rect2(x, y, slice_w, slice_h));
				ERR_CONTINUE(slice.is_null() || slice->is_empty());
				if (slice->get_width() != slice_w || slice->get_height() != slice_h) {
					slice->resize(slice_w, slice_h);
				}
				slices.push_back(slice);
			}
		}
		tex_3d = RS::get_singleton()->texture_3d_create(out_lut->get_format(), slice_w, slice_w, slice_w, false, slices);
	}
	// TODO iFire 2021-03-18 Remove test output
	out_lut->save_png("res://lut.png");
	return tex_3d;
}

ColorTransform::ColorTransform() {
	intent = CM_INTENT_PERCEPTUAL;
	use_bpc = true;
}

ColorTransform::ColorTransform(Ref<ColorProfile> p_src, Ref<ColorProfile> p_dst, Intent p_intent, bool p_black_point_compensation) {
	if (p_src.is_null() || !p_src->is_valid()) {
		ERR_PRINT("p_src is null or invalid.");
		return;
	}
	if (p_dst.is_null() || !p_dst->is_valid()) {
		ERR_PRINT("p_dst is null or invalid.");
		return;
	}

	src_profile = p_src;
	dst_profile = p_dst;
	intent = p_intent;
	use_bpc = p_black_point_compensation;
}

ColorTransform::~ColorTransform() {
}
