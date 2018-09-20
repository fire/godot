/*************************************************************************/
/*  open_simplex_noise.cpp                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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
#include "core/os/mutex.h"

OpenSimplexNoise::OpenSimplexNoise() {
	size.resize(4);
	for (size_t i = 0; i < size.size(); i++) {
		size.write[i] = 120;
	}
	seed = 0;
	period = 60.0f;
	persistence = 0.5f;
	octaves = 3;
	lacunarity = 2.0f;
	_init_seeds();
}

OpenSimplexNoise::~OpenSimplexNoise() {
}

void OpenSimplexNoise::_init_seeds() {
	ERR_FAIL_COND(size.size() < 4);
	ERR_FAIL_COND(size[0] < 6 && size[1] < 6 && size[2] < 6);
	int max = MAX(size[0], MAX(size[1], size[2]));
	max = max / real_t(6.0);
	for (int i = 0; i < 6; ++i) {
		open_simplex_noise(get_seed() + i * 2, max, max, max, &(contexts[i]));
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
	ERR_FAIL_COND(p_period < 1);
	period = p_period;
	_init_seeds();
	emit_changed();
}

float OpenSimplexNoise::get_period() const {
	return period;
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
	ERR_FAIL_COND_V(p_x < 1 || p_y < 1 || p_z < 1, Vector<Ref<Image> >());

	Vector<Ref<Image> > images;
	for (int d = 0; d < p_z; d++) {
		PoolVector<uint8_t> data;
		data.resize(p_x * p_y * 4);

		PoolVector<uint8_t>::Write wd8 = data.write();

		// Minimize the number of seedings
		int32_t original_seed = get_seed();
		for (int h = 0; h < p_y; h++) {
			for (int w = 0; w < p_x; w++) {
				uint8_t value;
				float v = get_noise_3d(d, h, w);
				v = v * 0.5 + 0.5; // Normalize [0..1]
				value = uint8_t(CLAMP(v * 255.0, 0, 255));

				wd8[(h * p_x + w) * 4 + 0] = value;
			}
		}

		set_seed(seed + 1);
		for (int h = 0; h < p_y; h++) {
			for (int w = 0; w < p_x; w++) {
				uint8_t value_2;
				float v2 = get_noise_3d(d, h, w);
				v2 = v2 * 0.5 + 0.5; // Normalize [0..1]
				value_2 = uint8_t(CLAMP(v2 * 255.0, 0, 255));

				wd8[(h * p_x + w) * 4 + 1] = value_2;
			}
		}

		set_seed(seed + 2);
		for (int h = 0; h < p_y; h++) {
			for (int w = 0; w < p_x; w++) {
				uint8_t value_3;
				float v3 = get_noise_3d(d, h, w);
				v3 = v3 * 0.5 + 0.5; // Normalize [0..1]
				value_3 = uint8_t(CLAMP(v3 * 255.0, 0, 255));

				wd8[(h * p_x + w) * 4 + 2] = value_3;
				wd8[(h * p_x + w) * 4 + 3] = 255;
			}
		}
		set_seed(original_seed);

		Ref<Image> image = memnew(Image(p_x, p_y, false, Image::FORMAT_RGBA8, data));
		images.push_back(image);
	}
	return images;
}

Vector<Ref<Image> > OpenSimplexNoise::get_seamless_image_3d(const int p_size) {
	ERR_FAIL_COND_V(p_size < 1, Vector<Ref<Image> >());

	Vector<Ref<Image> > images;
	for (int d = 0; d < p_size; d++) {
		PoolVector<uint8_t> data;
		data.resize(p_size * p_size * 4);

		PoolVector<uint8_t>::Write wd8 = data.write();

		// Minimize the number of seedings
		int32_t original_seed = get_seed();
		for (int h = 0; h < p_size; h++) {
			for (int w = 0; w < p_size; w++) {
				uint8_t value;
				float v = get_noise_3d_tiled(d, h, w);
				v = v * 0.5 + 0.5; // Normalize [0..1]
				value = uint8_t(CLAMP(v * 255.0, 0, 255));

				wd8[(h * p_size + w) * 4 + 0] = value;
			}
		}

		Mutex *mutex = Mutex::create();
		mutex->lock();
		set_seed(seed + 1);
		mutex->unlock();
		for (int h = 0; h < p_size; h++) {
			for (int w = 0; w < p_size; w++) {
				uint8_t value_2;
				float v2 = get_noise_3d_tiled(d, h, w);
				v2 = v2 * 0.5 + 0.5; // Normalize [0..1]
				value_2 = uint8_t(CLAMP(v2 * 255.0, 0, 255));

				wd8[(h * p_size + w) * 4 + 1] = value_2;
			}
		}
		mutex->lock();
		set_seed(seed + 2);
		mutex->unlock();
		for (int h = 0; h < p_size; h++) {
			for (int w = 0; w < p_size; w++) {
				uint8_t value_3;
				float v3 = get_noise_3d_tiled(d, h, w);
				v3 = v3 * 0.5 + 0.5; // Normalize [0..1]
				value_3 = uint8_t(CLAMP(v3 * 255.0, 0, 255));

				wd8[(h * p_size + w) * 4 + 2] = value_3;
				wd8[(h * p_size + w) * 4 + 3] = 255;
			}
		}
		mutex->lock();
		set_seed(original_seed);
		mutex->unlock();
		Ref<Image> image = memnew(Image(p_size, p_size, false, Image::FORMAT_RGBA8, data));
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

	ClassDB::bind_method(D_METHOD("get_seed"), &OpenSimplexNoise::get_seed);
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &OpenSimplexNoise::set_seed);

	ClassDB::bind_method(D_METHOD("set_octaves", "octave_count"), &OpenSimplexNoise::set_octaves);
	ClassDB::bind_method(D_METHOD("get_octaves"), &OpenSimplexNoise::get_octaves);

	ClassDB::bind_method(D_METHOD("set_period", "period"), &OpenSimplexNoise::set_period);
	ClassDB::bind_method(D_METHOD("get_period"), &OpenSimplexNoise::get_period);

	ClassDB::bind_method(D_METHOD("set_persistence", "persistence"), &OpenSimplexNoise::set_persistence);
	ClassDB::bind_method(D_METHOD("get_persistence"), &OpenSimplexNoise::get_persistence);

	ClassDB::bind_method(D_METHOD("set_lacunarity", "lacunarity"), &OpenSimplexNoise::set_lacunarity);
	ClassDB::bind_method(D_METHOD("get_lacunarity"), &OpenSimplexNoise::get_lacunarity);

	ClassDB::bind_method(D_METHOD("get_image", "width", "height"), &OpenSimplexNoise::get_image);
	ClassDB::bind_method(D_METHOD("get_seamless_image", "size"), &OpenSimplexNoise::get_seamless_image);

	ClassDB::bind_method(D_METHOD("get_noise_1d", "x"), &OpenSimplexNoise::get_noise_1d);
	ClassDB::bind_method(D_METHOD("get_noise_2d", "x", "y"), &OpenSimplexNoise::get_noise_2d);
	ClassDB::bind_method(D_METHOD("get_noise_3d", "x", "y", "z"), &OpenSimplexNoise::get_noise_3d);
	ClassDB::bind_method(D_METHOD("get_noise_4d", "x", "y", "z", "w"), &OpenSimplexNoise::get_noise_4d);

	ClassDB::bind_method(D_METHOD("get_noise_2dv", "pos"), &OpenSimplexNoise::get_noise_2dv);
	ClassDB::bind_method(D_METHOD("get_noise_3dv", "pos"), &OpenSimplexNoise::get_noise_3dv);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "octaves", PROPERTY_HINT_RANGE, "1,6,1"), "set_octaves", "get_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "period", PROPERTY_HINT_RANGE, "0.1,256.0,0.1"), "set_period", "get_period");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "persistence", PROPERTY_HINT_RANGE, "0.0,1.0,0.001"), "set_persistence", "get_persistence");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "lacunarity", PROPERTY_HINT_RANGE, "0.1,4.0,0.01"), "set_lacunarity", "get_lacunarity");
}

float OpenSimplexNoise::get_noise_1d(float x) {

	return get_noise_2d(x, 1.0);
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
	x /= period;
	y /= period;
	z /= period;

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

float OpenSimplexNoise::get_noise_3d_tiled(float x, float y, float z) {
	x /= period;
	y /= period;
	z /= period;

	float amp = 1.0;
	float max = 1.0;
	float sum = _get_octave_noise_3d_tiled(0, x, y, z);

	int i = 0;
	while (++i < octaves) {
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
		amp *= persistence;
		max += amp;
		sum += _get_octave_noise_3d_tiled(i, x, y, z) * amp;
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
