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

void NoiseTexture3D::_set_texture_data(const Ref<Image> &p_image, const int p_layer) {
	if (data.size() != size.z) {
		data.resize(size.z);
		VS::get_singleton()->texture_allocate(texture, size.x, size.y, size.z, Image::FORMAT_RGBA8, VS::TEXTURE_TYPE_3D, flags);
	}
	if (p_image.is_valid()) {
		data.insert(p_layer, p_image);
		VS::get_singleton()->texture_set_data(texture, p_image, p_layer);
		emit_changed();
	}
}

void NoiseTexture3D::_thread_done(const Ref<Image> &p_image, const int p_layer) {

	_set_texture_data(p_image, p_layer);
	Thread::wait_to_finish(noise_thread);
	if (p_layer == size.z) {
		memdelete(noise_thread);
		noise_thread = NULL;
	}
	if (regen_queued) {
		noise_thread = Thread::create(_thread_function, this);
		regen_queued = false;
	}
}

void NoiseTexture3D::_thread_function(void *p_ud) {

	NoiseTexture3D *tex = (NoiseTexture3D *)p_ud;

	for (size_t i = 0; i < tex->get_length(); i++) {
		tex->call_deferred("_thread_done", tex->_generate_texture(i));
	}
}

void NoiseTexture3D::_queue_update() {

	if (update_queued)
		return;

	update_queued = true;
	call_deferred("_update_texture");
}

Ref<Image> NoiseTexture3D::_generate_texture(const int p_layer) {

	update_queued = false;

	if (noise.is_null()) return Ref<Image>();
	Ref<Image> image = noise->get_image_3d(size.x, size.y, size.z, p_layer);

	return image;
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
// @TODO FIX THREADING
	use_thread = false;
	if (use_thread) {

		if (!noise_thread) {
			noise_thread = Thread::create(_thread_function, this);
			regen_queued = false;
		} else {
			regen_queued = true;
		}

	} else {
		for (size_t i = 0; i < get_length(); i++) {
			Ref<Image> image = _generate_texture(i);
			_set_texture_data(image, i);
		}
	}
}

void NoiseTexture3D::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_width", "width"), &NoiseTexture3D::set_width);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &NoiseTexture3D::set_height);
	ClassDB::bind_method(D_METHOD("set_length", "length"), &NoiseTexture3D::set_length);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &NoiseTexture3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &NoiseTexture3D::get_size);

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &NoiseTexture3D::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &NoiseTexture3D::get_noise);

	ClassDB::bind_method(D_METHOD("_update_texture"), &NoiseTexture3D::_update_texture);
	ClassDB::bind_method(D_METHOD("_generate_texture"), &NoiseTexture3D::_generate_texture);
	ClassDB::bind_method(D_METHOD("_thread_done", "image"), &NoiseTexture3D::_thread_done);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "OpenSimplexNoise"), "set_noise", "get_noise");
}

NoiseTexture3D::NoiseTexture3D() {
	update_queued = false;
	noise_thread = NULL;
	regen_queued = false;
	first_time = true;

	size = Vector3(128, 128, 128);
	flags = FLAGS_DEFAULT;

	noise = Ref<OpenSimplexNoise>();

	texture = VS::get_singleton()->texture_create();

	_queue_update();
}

NoiseTexture3D::~NoiseTexture3D() {
	VS::get_singleton()->free(texture);
}

void NoiseTexture3D::set_noise(Ref<OpenSimplexNoise> p_noise) {
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

Ref<OpenSimplexNoise> NoiseTexture3D::get_noise() {
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
