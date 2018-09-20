/*************************************************************************/
/*  fractal_brownian_noise.h                                             */
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

#ifndef FRACTAL_BROWNIAN_NOISE_H
#define FRACTAL_BROWNIAN_NOISE_H

#include "core/image.h"
#include "core/reference.h"
#include "scene/resources/noise.h"
#include "scene/resources/texture.h"

class FractalBrownianNoise : public Noise {
	GDCLASS(FractalBrownianNoise, Noise)
	OBJ_SAVE_TYPE(FractalBrownianNoise);

public:
	FractalBrownianNoise();
	~FractalBrownianNoise();

	virtual void set_seed(int seed) = 0;
	virtual int get_seed() const = 0;

	virtual void set_period(float p_period) = 0;
	virtual float get_period() const = 0;

	virtual void set_size(Vector<int32_t> p_size) = 0;
	virtual Vector<int32_t> get_size() const = 0;

	virtual void set_octaves(int p_octaves) = 0;
	virtual int get_octaves() const = 0;

	virtual void set_persistence(float p_persistence) = 0;
	virtual float get_persistence() const = 0;

	virtual void set_lacunarity(float p_lacunarity) = 0;
	virtual float get_lacunarity() const = 0;

protected:
	static void _bind_methods();
};

#endif // FRACTAL_BROWNIAN_NOISE_H
