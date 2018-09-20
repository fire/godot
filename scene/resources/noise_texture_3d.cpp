#include "noise_texture_3d.h"
/*************************************************************************/
/*  noise_texture_3d.cpp                                                 */
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

#include "noise_texture.h"

#include "core/core_string_names.h"
#include "core/method_bind_ext.gen.inc"
#include "noise_texture_3d.h"
#include "scene/resources/noise.h"

void NoiseTexture3D::_set_texture_data(const Vector<Ref<Image> > data_layers) {
	ERR_FAIL_COND(size.z != data_layers.size());
	ERR_FAIL_COND(int32_t(size.z) % 6 != 0);
	data = data_layers;
	VS::get_singleton()->texture_allocate(texture, size.x, size.y, size.z, Image::FORMAT_RGBA8, VS::TEXTURE_TYPE_3D, get_flags());
	for (size_t i = 0; i < data_layers.size(); i++) {
		VS::get_singleton()->texture_set_data(texture, data_layers[i], i);
	}
	emit_changed();
}

void NoiseTexture3D::_thread_done() {
	Thread::wait_to_finish(noise_thread);
	memdelete(noise_thread);
	noise_thread = NULL;
	if (regen_queued) {
		noise_thread = Thread::create(_thread_function, this);
		regen_queued = false;
	}
}

void NoiseTexture3D::_thread_function(void *p_ud) {

	NoiseTexture3D *tex = (NoiseTexture3D *)p_ud;

	Vector<Ref<Image> > image_layers = tex->_generate_texture();
	tex->_set_texture_data(image_layers);

	tex->call_deferred("_thread_done");
}


void NoiseTexture3D::set_seamless(bool p_seamless) {
	if (p_seamless == seamless) return;
	seamless = p_seamless;
	_queue_update();
}

bool NoiseTexture3D::get_seamless() {
	return seamless;
}

void NoiseTexture3D::_queue_update() {

	if (update_queued)
		return;

	update_queued = true;
	call_deferred("_update_texture");
}

Vector<Ref<Image>> NoiseTexture3D::_generate_texture() {
	ERR_FAIL_COND_V(noise == NULL, Vector<Ref<Image> >());
	update_queued = false;
	Vector<int32_t> size_list;
	size_list.push_back(size.x);
	size_list.push_back(size.y);
	size_list.push_back(size.z);
	size_list.push_back(1);
	noise->set_size(size_list);
	int32_t max = MAX(size.x, MAX(size.y, size.z));
	if (seamless) {
		return noise->get_seamless_image_3d(max);
	}
	return noise->get_image_3d(size.x, size.y, size.z);
}

void NoiseTexture3D::_update_texture() {
	bool use_thread = true;
	if (first_time) {
		use_thread = false;
		first_time = false;
	}
#ifdef NO_THREADS
	use_thread = false;
#endif
	if (use_thread) {

		if (!noise_thread) {
			data.clear();
			noise_thread = Thread::create(_thread_function, this);
			regen_queued = false;
		} else {
			regen_queued = true;
		}

	} else {
		Vector<Ref<Image> > image_layers = _generate_texture();
		_set_texture_data(image_layers);
	}
}

void NoiseTexture3D::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_width", "width"), &NoiseTexture3D::set_width);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &NoiseTexture3D::set_height);
	ClassDB::bind_method(D_METHOD("set_length", "length"), &NoiseTexture3D::set_length);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &NoiseTexture3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &NoiseTexture3D::get_size);
	ClassDB::bind_method(D_METHOD("set_seamless", "seamless"), &NoiseTexture3D::set_seamless);
	ClassDB::bind_method(D_METHOD("get_seamless"), &NoiseTexture3D::get_seamless);

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &NoiseTexture3D::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &NoiseTexture3D::get_noise);

	ClassDB::bind_method(D_METHOD("_update_texture"), &NoiseTexture3D::_update_texture);
	ClassDB::bind_method(D_METHOD("_thread_done"), &NoiseTexture3D::_thread_done);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_RANGE, "1,1024,1"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "seamless"), "set_seamless", "get_seamless");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FractalBrownianNoise"), "set_noise", "get_noise");
}

NoiseTexture3D::NoiseTexture3D() {
	update_queued = false;
	noise_thread = NULL;
	regen_queued = false;
	first_time = true;
	seamless = true;

	size = Vector3(120, 120, 120);
	flags = FLAGS_DEFAULT;

	noise = Ref<FractalBrownianNoise>();

	texture = VS::get_singleton()->texture_create();

	_queue_update();
}

NoiseTexture3D::~NoiseTexture3D() {
	VS::get_singleton()->free(texture);
}

void NoiseTexture3D::set_noise(Ref<FractalBrownianNoise> p_noise) {
	if (p_noise == noise)
		return;
	if (noise.is_valid()) {
		noise->disconnect(CoreStringNames::get_singleton()->changed, this, "_update_texture");
	}
	noise = p_noise;
	if (noise.is_valid()) {
		noise->connect(CoreStringNames::get_singleton()->changed, this, "_update_texture");
	}
	_queue_update();
}

Ref<FractalBrownianNoise> NoiseTexture3D::get_noise() {
	return noise;
}

void NoiseTexture3D::set_width(int p_width) {
	if (p_width == size.x) return;
	size.x = p_width;
	_queue_update();
}

void NoiseTexture3D::set_height(int p_height) {
	if (p_height == size.y) return;
	size.y = p_height;
	_queue_update();
}

void NoiseTexture3D::set_length(int p_length) {
	if (p_length == size.z) return;
	size.z = p_length;
	_queue_update();
}

void NoiseTexture3D::set_size(Vector3 p_size) {
	if (p_size == size) return;
	size = p_size;
	_queue_update();
}

Vector3 NoiseTexture3D::get_size() {

	return size;
}

int NoiseTexture3D::get_width() const {

	return size.x;
}

int NoiseTexture3D::get_height() const {

	return size.y;
}

int NoiseTexture3D::get_length() const {

	return size.z;
}

void NoiseTexture3D::set_flags(uint32_t p_flags) {
	flags = p_flags;
	VS::get_singleton()->texture_set_flags(texture, flags);
}

uint32_t NoiseTexture3D::get_flags() const {
	return flags;
}

Vector<Ref<Image> > NoiseTexture3D::get_data() const {

	return data;
}
