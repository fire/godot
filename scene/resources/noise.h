/*************************************************************************/
/*  noise.h                                                              */
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

#ifndef NOISE_H
#define NOISE_H

#include "core/image.h"
#include "core/reference.h"
#include "scene/resources/texture.h"

class Noise : public Resource {
	GDCLASS(Noise, Resource)
	OBJ_SAVE_TYPE(Noise);

public:
	Noise();
	~Noise();

	virtual void set_seed(int seed) = 0;
	virtual int get_seed() const = 0;

	virtual Ref<Image> get_image(int p_width, int p_height) = 0;
	virtual Vector<Ref<Image> > get_image_3d(int p_x, int p_y, int p_z) = 0;
	virtual Ref<Image> get_seamless_image(int p_size) = 0;
	virtual Vector<Ref<Image> > get_seamless_image_3d(int p_size) = 0;
	virtual float get_noise_2d(float x, float y) = 0;
	virtual float get_noise_3d(float x, float y, float z) = 0;
	virtual float get_noise_4d(float x, float y, float z, float w) = 0;

	_FORCE_INLINE_ float get_noise_2dv(Vector2 v) { return get_noise_2d(v.x, v.y); }
	_FORCE_INLINE_ float get_noise_3dv(Vector3 v) { return get_noise_3d(v.x, v.y, v.z); }

protected:
	static void _bind_methods();
};

#endif // NOISE_H
