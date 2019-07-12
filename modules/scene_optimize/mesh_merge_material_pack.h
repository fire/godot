/*************************************************************************/
/*  mesh_merge_material_repack.h                                         */
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

/*
xatlas
https://github.com/jpcy/xatlas
Copyright (c) 2018 Jonathan Young
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/*
thekla_atlas
https://github.com/Thekla/thekla_atlas
MIT License
Copyright (c) 2013 Thekla, Inc
Copyright NVIDIA Corporation 2006 -- Ignacio Castano <icastano@nvidia.com>
*/

#ifndef MESH_MERGE_MATERIAL_REPACK_H
#define MESH_MERGE_MATERIAL_REPACK_H

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <stdlib.h>
#include "thirdparty/xatlas/xatlas.h"

#ifdef _MSC_VER
#define FOPEN(_file, _filename, _mode)                            \
	{                                                             \
		if (fopen_s(&_file, _filename, _mode) != 0) _file = NULL; \
	}
#define STRCAT(_dest, _size, _src) strcat_s(_dest, _size, _src);
#define STRCPY(_dest, _size, _src) strcpy_s(_dest, _size, _src);
#define STRICMP _stricmp
#else
#define FOPEN(_file, _filename, _mode) _file = fopen(_filename, _mode)
#include <string.h>
#include <strings.h>
#define STRCAT(_dest, _size, _src) strcat(_dest, _src);
#define STRCPY(_dest, _size, _src) strcpy(_dest, _src);
#define STRICMP strcasecmp
#endif

#include "core/math/vector2.h"
#include "core/reference.h"
#include "scene/3d/mesh_instance.h"
#include "scene_optimize.h"

class MeshMergeMaterialRepack : public Reference {
private:
	struct TextureData {
		uint16_t width;
		uint16_t height;
		int numComponents;
		Ref<Image> image;
	};

	class ClippedTriangle {
	public:
		ClippedTriangle(const Vector2 &a, const Vector2 &b, const Vector2 &c);

		void clipHorizontalPlane(float offset, float clipdirection);

		void clipVerticalPlane(float offset, float clipdirection);

		void computeAreaCentroid();

		void clipAABox(float x0, float y0, float x1, float y1);

		Vector2 centroid();

		float area();

	private:
		Vector2 m_verticesA[7 + 1];
		Vector2 m_verticesB[7 + 1];
		Vector2 *m_vertexBuffers[2];
		uint32_t m_numVertices;
		uint32_t m_activeVertexBuffer;
		float m_area;
		Vector2 m_centroid;
	};

	/// A callback to sample the environment. Return false to terminate rasterization.
	typedef bool (*SamplingCallback)(void *param, int x, int y, const Vector3 &bar, const Vector3 &dx, const Vector3 &dy, float coverage);

	struct Triangle {
		Triangle(const Vector2 &v0, const Vector2 &v1, const Vector2 &v2, const Vector3 &t0, const Vector3 &t1, const Vector3 &t2);

		/// Compute texture space deltas.
		/// This method takes two edge vectors that form a basis, determines the
		/// coordinates of the canonic vectors in that basis, and computes the
		/// texture gradient that corresponds to those vectors.
		bool computeDeltas();

		void flipBackface();

		// compute unit inward normals for each edge.
		void computeUnitInwardNormals();

		bool drawAA(SamplingCallback cb, void *param);

		Vector2 v1, v2, v3;
		Vector2 n1, n2, n3; // unit inward normals
		Vector3 t1, t2, t3;
		Vector3 dx, dy;
	};

	struct SetAtlasTexelArgs {
		Ref<Image> atlasData;
		Vector2 sourceUv[3];
		Ref<Image> sourceTexture;
	};

	static bool setAtlasTexel(void *param, int x, int y, const Vector3 &bar, const Vector3 &, const Vector3 &, float);

	struct ModelVertex {
		Vector3 pos;
		Vector3 normal;
		Vector2 uv;
		uint32_t index;
	};

public:
	void pack(Vector<SceneOptimize::MeshInfo> &mesh_items, String p_scene_name);
	void generate_atlas(const int32_t p_num_meshes, PoolVector2Array &r_uvs, xatlas::Atlas *atlas, Vector<SceneOptimize::MeshInfo>&r_meshes);
	void scale_uvs_by_texture_dimension(Vector<SceneOptimize::MeshInfo>&mesh_items, PoolVector2Array &uvs, PoolVector<Ref<Material>>&r_vertex_to_material, Vector<ModelVertex> &r_model_vertices);
	void map_vertex_to_material(Vector<SceneOptimize::MeshInfo>mesh_items, PoolVector<Ref<Material>> &vertex_to_material);
	void output(xatlas::Atlas *atlas, Vector<SceneOptimize::MeshInfo> &r_mesh_items, const PoolVector<Ref<Material>> vertex_to_material, const PoolVector2Array uvs, const Vector<ModelVertex> model_vertices, String p_name);
};
#endif
