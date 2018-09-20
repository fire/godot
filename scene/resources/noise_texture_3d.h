/*************************************************************************/
/*  noise_texture_3d.h                                                   */
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

#ifndef NOISE_TEXTURE_3D_H
#define NOISE_TEXTURE_3D_H

#include "core/core_string_names.h"
#include "core/image.h"
#include "core/reference.h"
#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "editor/property_editor.h"
#include "scene/resources/fractal_brownian_noise.h"

class NoiseTexture3D : public Texture3D {
	GDCLASS(NoiseTexture3D, Texture3D)

private:
	struct ImageLayer {
		Ref<NoiseTexture3D> ref;
		int layer;
	} noise_thread_layer;

	Vector<Ref<Image> > data;

	Thread *noise_thread;

	bool first_time;
	bool update_queued;
	bool regen_queued;
	bool seamless;

	RID texture;
	uint32_t flags;

	Ref<FractalBrownianNoise> noise;
	Vector3 size;

	void _set_texture_data(const Vector<Ref<Image> > data_layers);
	void _thread_done();
	static void _thread_function(void *p_ud);

	void _queue_update();
	Vector<Ref<Image>> _generate_texture();
	void _update_texture();

protected:
	static void _bind_methods();

public:
	void set_noise(Ref<FractalBrownianNoise> p_noise);
	Ref<FractalBrownianNoise> get_noise();
	void set_width(int p_width);
	void set_height(int p_height);
	void set_length(int p_length);
	void set_seamless(bool p_seamless);
	bool get_seamless();
	void set_size(Vector3 p_size);
	Vector3 get_size();
	int get_width() const;
	int get_height() const;
	int get_length() const;
	void set_flags(uint32_t p_flags);
	uint32_t get_flags() const;
	Vector<Ref<Image> > get_data() const;
	NoiseTexture3D();
	~NoiseTexture3D();
};

#endif // NOISE_TEXTURE_3D_H
