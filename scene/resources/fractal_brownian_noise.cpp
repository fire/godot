/*************************************************************************/
/*  fractal_brownian_noise.cpp                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "scene/resources/fractal_brownian_noise.h"

#include "core/core_string_names.h"

FractalBrownianNoise::FractalBrownianNoise() {
}

FractalBrownianNoise::~FractalBrownianNoise() {
}

void FractalBrownianNoise::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_octaves", "octave_count"), &FractalBrownianNoise::set_octaves);
	ClassDB::bind_method(D_METHOD("get_octaves"), &FractalBrownianNoise::get_octaves);

	ClassDB::bind_method(D_METHOD("set_period", "period"), &FractalBrownianNoise::set_period);
	ClassDB::bind_method(D_METHOD("get_period"), &FractalBrownianNoise::get_period);

	ClassDB::bind_method(D_METHOD("set_size", "size"), &FractalBrownianNoise::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &FractalBrownianNoise::get_size);

	ClassDB::bind_method(D_METHOD("set_persistence", "persistence"), &FractalBrownianNoise::set_persistence);
	ClassDB::bind_method(D_METHOD("get_persistence"), &FractalBrownianNoise::get_persistence);

	ClassDB::bind_method(D_METHOD("set_lacunarity", "lacunarity"), &FractalBrownianNoise::set_lacunarity);
	ClassDB::bind_method(D_METHOD("get_lacunarity"), &FractalBrownianNoise::get_lacunarity);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "octaves", PROPERTY_HINT_RANGE, "1,6,1"), "set_octaves", "get_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "period", PROPERTY_HINT_RANGE, "1.0,8096,1.0"), "set_period", "get_period");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "size", PROPERTY_HINT_NONE, "1,1,4", PROPERTY_USAGE_NOEDITOR), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "persistence", PROPERTY_HINT_RANGE, "0.0,1.0,0.001"), "set_persistence", "get_persistence");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "lacunarity", PROPERTY_HINT_RANGE, "0.1,4.0,0.01"), "set_lacunarity", "get_lacunarity");
}
