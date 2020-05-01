/*************************************************************************/
/*  subdivision.h                                                        */
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

#ifndef SUBDIVISION_H
#define SUBDIVISION_H

#include "core/rid.h"

#include "scene/resources/mesh.h"

class MeshSubdivision {
public:
	MeshSubdivision() {}
	virtual ~MeshSubdivision() {}

	virtual RID get_rid() const = 0;

	virtual void update_subdivision(Ref<Mesh> p_mesh, int p_level) = 0;
	virtual void update_skinning(RID p_skeleton) = 0;
};

class SubdivisionSystem {
	static SubdivisionSystem *singleton;

public:
	static void register_subdivision_system(SubdivisionSystem *p_system);
	static void unregister_subdivision_system(SubdivisionSystem *p_system);
	static SubdivisionSystem *get_subdivision_system() { return singleton; }

	virtual MeshSubdivision *create_mesh_subdivision(Ref<Mesh> p_mesh, int p_level) = 0;
	virtual void destroy_mesh_subdivision(MeshSubdivision *p_mesh_subdivision) = 0;
};

#endif // SUBDIVISION_H
