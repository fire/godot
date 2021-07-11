/*************************************************************************/
/*  color_profile.cpp                                                    */
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

#include "color_profile.h"
#include "core/object/ref_counted.h"

bool ColorProfile::is_valid() {
	return profile_valid && profile != NULL;
}

void ColorProfile::_set_profile(cmsHPROFILE p_profile) {
	if (is_valid()) {
		cmsCloseProfile(profile);
	}

	profile = p_profile;
	profile_valid = p_profile != NULL;
}

ColorProfile::Handle ColorProfile::get_profile_handle() {
	if (!is_valid()) {
		ERR_PRINT("Trying to call get_profile_handle() on an invalid ColorProfile instance.");
		return Handle();
	}
	return Handle(profile);
}

void ColorProfile::load_predef(Predef p_predef) {
	switch (p_predef) {
		case PREDEF_LINEAR: {
			cmsCIExyY d65_srgb_adobe_specs = { 0.3127, 0.3290, 1.0 };
			// https://invent.kde.org/graphics/krita/-/blob/63fc02052cd7b84d6bbe60df238fd7cf2395f64d/krita/data/profiles/elles-icc-profiles/README
			cmsCIExyYTRIPLE srgb_primaries_pre_quantized = {
				{ 0.639998686, 0.330010138, 1.0 },
				{ 0.300003784, 0.600003357, 1.0 },
				{ 0.150002046, 0.059997204, 1.0 }
			};
			cmsToneCurve *gamma[3];
			gamma[0] = gamma[1] = gamma[2] = cmsBuildGamma(nullptr, 1.0f);
			cmsHPROFILE new_profile = cmsCreateRGBProfile(&d65_srgb_adobe_specs, &srgb_primaries_pre_quantized, gamma);
			_set_profile(new_profile);
			break;
		}
		default: {
			ERR_PRINT("ColorProfile creation failure: Unknown predefined profile\n");
			break;
		}
	}
}

void ColorProfile::load_from_file(const String &p_file) {
	CharString utf_path = p_file.utf8();
	cmsHPROFILE p = cmsOpenProfileFromFile(utf_path.ptr(), "r");
	if (p == NULL) {
		ERR_PRINT("Failed to load ICC profile from file: " + p_file + ".");
	}
	_set_profile(p);
}

ColorProfile::ColorProfile() {
	profile_valid = false;
}

ColorProfile::ColorProfile(Predef p_predef) {
	profile_valid = false;
	load_predef(p_predef);
}

ColorProfile::ColorProfile(const String &p_file) {
	profile_valid = false;
	load_from_file(p_file);
}

ColorProfile::~ColorProfile() {
	if (is_valid()) {
		cmsCloseProfile(profile);
	}
}
