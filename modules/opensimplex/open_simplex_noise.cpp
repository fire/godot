/*************************************************************************/
/*  open_simplex_noise.cpp                                               */
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

#include "open_simplex_noise.h"

#include "core/core_string_names.h"

OpenSimplexNoise::OpenSimplexNoise() {
	size.resize(4);
	for (size_t i = 0; i < size.size(); i++) {
		size.write[i] = 1;
	}
	seed = 0;
	period = 64.0f;
	const int32_t size_num = 4;
	const int32_t period_num = size_num;
	seamless_period.resize(period_num);
	for (size_t i = 0; i < period_num; i++) {
		seamless_period.write[i] = 64;
	}
	persistence = 0.5;
	octaves = 3;
	lacunarity = 2.0;
	_init_seeds();
}

OpenSimplexNoise::~OpenSimplexNoise() {
}

void OpenSimplexNoise::_init_seeds() {
	float scale = 1.0f / 6.0f;
	for (int i = 0; i < 6; ++i) {
		open_simplex_noise(get_seed(), seamless_period[0] * scale, seamless_period[1] * scale, seamless_period[2] * scale, &(contexts[i]));
	}
}

void OpenSimplexNoise::set_seed(int p_seed) {
	if (seed == p_seed)
		return;

	seed = p_seed;
	_init_seeds();
	emit_changed();
}

int OpenSimplexNoise::get_seed() const {

	return seed;
}

void OpenSimplexNoise::set_size(Vector<int32_t> p_size) {
	ERR_FAIL_COND(p_size.size() < 4);
	size = p_size;
}

Vector<int32_t> OpenSimplexNoise::get_size() const {
	return size;
}

void OpenSimplexNoise::set_octaves(int p_octaves) {
	if (p_octaves == octaves) return;
	octaves = CLAMP(p_octaves, 1, 6);
	emit_changed();
}

void OpenSimplexNoise::set_period(float p_period) {
	period = p_period;
	_init_seeds();
	emit_changed();
}

float OpenSimplexNoise::get_period() const {
	return period;
}

void OpenSimplexNoise::set_seamless_period(Vector<int32_t> p_seamless_period) {
	ERR_FAIL_COND(p_seamless_period.size() != 4)
	ERR_FAIL_COND(seamless_period.size() != 4)
	ERR_FAIL_COND(seamless_period[0] < 6 && seamless_period[1] < 6 && seamless_period[2] < 6 && seamless_period[3] < 6);
	for (size_t i = 0; i < p_seamless_period.size(); i++) {
		seamless_period.write[i] = p_seamless_period[i];
	}
	_init_seeds();
	emit_changed();
}

Vector<int32_t> OpenSimplexNoise::get_seamless_period() const {
	return seamless_period;
}

void OpenSimplexNoise::set_persistence(float p_persistence) {
	if (p_persistence == persistence) return;
	persistence = p_persistence;
	emit_changed();
}

void OpenSimplexNoise::set_lacunarity(float p_lacunarity) {

	if (p_lacunarity == lacunarity) return;
	lacunarity = p_lacunarity;
	emit_changed();
}

Ref<Image> OpenSimplexNoise::get_image(int p_width, int p_height) {

	PoolVector<uint8_t> data;
	data.resize(p_width * p_height * 4);

	PoolVector<uint8_t>::Write wd8 = data.write();

	for (int i = 0; i < p_height; i++) {
		for (int j = 0; j < p_width; j++) {
			float v = get_noise_2d(i, j);
			v = v * 0.5 + 0.5; // Normalize [0..1]
			uint8_t value = uint8_t(CLAMP(v * 255.0, 0, 255));
			wd8[(i * p_width + j) * 4 + 0] = value;
			wd8[(i * p_width + j) * 4 + 1] = value;
			wd8[(i * p_width + j) * 4 + 2] = value;
			wd8[(i * p_width + j) * 4 + 3] = 255;
		}
	}

	Ref<Image> image = memnew(Image(p_width, p_height, false, Image::FORMAT_RGBA8, data));
	return image;
}

Vector<Ref<Image> > OpenSimplexNoise::get_image_3d(int p_x, int p_y, int p_z) {
	ERR_FAIL_COND_V(p_x < 1, Vector<Ref<Image> >());
	Vector<int32_t> period;
	Vector<Ref<Image> > images;
	for (int d = 0; d < p_z; d++) {
		PoolVector<uint8_t> data;
		data.resize(p_x * p_y * 4);
		const int32_t depth = p_z - 1;

		PoolVector<uint8_t>::Write wd8 = data.write();
		for (int h = 0; h < p_y; h++) {
			for (int w = 0; w < p_x; w++) {
				uint8_t value;

				float v = get_noise_3d(w, h, depth);
				v = v * 0.5 + 0.5; // Normalize [0..1]
				value = uint8_t(CLAMP(v * 255.0, 0, 255));

				wd8[(h * p_x + w) * 4 + 0] = value; // @TODO generate 3 different values at a time
				wd8[(h * p_x + w) * 4 + 1] = value;
				wd8[(h * p_x + w) * 4 + 2] = value;
				wd8[(h * p_x + w) * 4 + 3] = 255;
			}
		}
		Ref<Image> image = memnew(Image(p_x, p_y, false, Image::FORMAT_RGBA8, data));

		images.push_back(image);
	}
	return images;
}

Vector<Ref<Image> > OpenSimplexNoise::get_seamless_image_3d(const int p_size) {
	ERR_FAIL_COND_V(p_size < 1, Vector<Ref<Image> >());
	const int p_x = p_size;
	const int p_y = p_size;
	const int p_z = p_size;

	Vector<int32_t> period;
	Vector<Ref<Image> > images;
	for (int d = 0; d < p_z; d++) {
		PoolVector<uint8_t> data;
		data.resize(p_x * p_y * 4);
		const int32_t depth = p_z - 1;

		PoolVector<uint8_t>::Write wd8 = data.write();
		for (int h = 0; h < p_y; h++) {
			for (int w = 0; w < p_x; w++) {
				uint8_t value;

				float v = get_noise_3d(w, h, depth);
				v = v * 0.5 + 0.5; // Normalize [0..1]
				value = uint8_t(CLAMP(v * 255.0, 0, 255));

				wd8[(h * p_x + w) * 4 + 0] = value; // @TODO generate 3 different values at a time
				wd8[(h * p_x + w) * 4 + 1] = value;
				wd8[(h * p_x + w) * 4 + 2] = value;
				wd8[(h * p_x + w) * 4 + 3] = 255;
			}
		}
		Ref<Image> image = memnew(Image(p_x, p_y, false, Image::FORMAT_RGBA8, data));

		images.push_back(image);
	}
	return images;
}

Ref<Image> OpenSimplexNoise::get_seamless_image(int p_size) {

	PoolVector<uint8_t> data;
	data.resize(p_size * p_size * 4);

	PoolVector<uint8_t>::Write wd8 = data.write();

	for (int i = 0; i < p_size; i++) {
		for (int j = 0; j < p_size; j++) {

			float ii = (float)i / (float)p_size;
			float jj = (float)j / (float)p_size;

			ii *= 2.0 * Math_PI;
			jj *= 2.0 * Math_PI;

			float radius = p_size / (2.0 * Math_PI);

			float x = radius * Math::sin(jj);
			float y = radius * Math::cos(jj);
			float z = radius * Math::sin(ii);
			float w = radius * Math::cos(ii);
			float v = get_noise_4d(x, y, z, w);

			v = v * 0.5 + 0.5; // Normalize [0..1]
			uint8_t value = uint8_t(CLAMP(v * 255.0, 0, 255));
			wd8[(i * p_size + j) * 4 + 0] = value;
			wd8[(i * p_size + j) * 4 + 1] = value;
			wd8[(i * p_size + j) * 4 + 2] = value;
			wd8[(i * p_size + j) * 4 + 3] = 255;
		}
	}

	Ref<Image> image = memnew(Image(p_size, p_size, false, Image::FORMAT_RGBA8, data));
	return image;
}

void OpenSimplexNoise::_bind_methods() {
}

float OpenSimplexNoise::get_noise_2d(float x, float y) {

	x /= period;
	y /= period;

	float amp = 1.0;
	float max = 1.0;
	float sum = _get_octave_noise_2d(0, x, y);

	int i = 0;
	while (++i < octaves) {
		x *= lacunarity;
		y *= lacunarity;
		amp *= persistence;
		max += amp;
		sum += _get_octave_noise_2d(i, x, y) * amp;
	}

	return sum / max;
}

float OpenSimplexNoise::get_noise_3d(float x, float y, float z) {

	//x /= period;
	//y /= period;
	//z /= period;

	float amp = 1.0;
	float max = 1.0;
	float sum = _get_octave_noise_3d(0, x, y, z);

	int i = 0;
	while (++i < octaves) {
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
		amp *= persistence;
		max += amp;
		sum += _get_octave_noise_3d(i, x, y, z) * amp;
	}

	return sum / max;
}

float OpenSimplexNoise::get_noise_4d(float x, float y, float z, float w) {

	x /= period;
	y /= period;
	z /= period;
	w /= period;

	float amp = 1.0;
	float max = 1.0;
	float sum = _get_octave_noise_4d(0, x, y, z, w);

	int i = 0;

	while (++i < octaves) {
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
		w *= lacunarity;
		amp *= persistence;
		max += amp;
		sum += _get_octave_noise_4d(i, x, y, z, w) * amp;
	}

	return sum / max;
}
