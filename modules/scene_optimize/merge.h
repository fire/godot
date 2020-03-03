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

#include "thirdparty/xatlas/xatlas.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <cmath>
#include <vector>

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
#include "optimize.h"
#include "scene/3d/mesh_instance.h"

class MeshMergeMaterialRepack : public Reference {
private:

	struct TextureData {
		uint16_t width;
		uint16_t height;
		int numComponents;
		Ref<Image> image;
	};

	/// A callback to sample the environment. Return false to terminate rasterization.
	typedef bool (*SamplingCallback)(void *param, int x, int y, const Vector3 &bar, const Vector3 &dx, const Vector3 &dy, float coverage);

	struct Triangle {
		Triangle(const Vector2 &v0, const Vector2 &v1, const Vector2 &v2, const Vector3 &t0, const Vector3 &t1, const Vector3 &t2) {
			// Init vertices.
			this->v1 = v0;
			this->v2 = v2;
			this->v3 = v1;
			// Set barycentric coordinates.
			this->t1 = t0;
			this->t2 = t2;
			this->t3 = t1;
			// make sure every triangle is front facing.
			flipBackface();
			// Compute deltas.
			computeDeltas();
			computeUnitInwardNormals();
		}

		/// Compute texture space deltas.
		/// This method takes two edge vectors that form a basis, determines the
		/// coordinates of the canonic vectors in that basis, and computes the
		/// texture gradient that corresponds to those vectors.
		bool computeDeltas() {
			Vector2 e0 = v3 - v1;
			Vector2 e1 = v2 - v1;
			Vector3 de0 = t3 - t1;
			Vector3 de1 = t2 - t1;
			float denom = 1.0f / (e0.y * e1.x - e1.y * e0.x);
			if (!std::isfinite(denom)) {
				return false;
			}
			float lambda1 = -e1.y * denom;
			float lambda2 = e0.y * denom;
			float lambda3 = e1.x * denom;
			float lambda4 = -e0.x * denom;
			dx = de0 * lambda1 + de1 * lambda2;
			dy = de0 * lambda3 + de1 * lambda4;
			return true;
		}

		void flipBackface() {
			// check if triangle is backfacing, if so, swap two vertices
			if (((v3.x - v1.x) * (v2.y - v1.y) - (v3.y - v1.y) * (v2.x - v1.x)) < 0) {
				Vector2 hv = v1;
				v1 = v2;
				v2 = hv; // swap pos
				Vector3 ht = t1;
				t1 = t2;
				t2 = ht; // swap tex
			}
		}

		// compute unit inward normals for each edge.
		void computeUnitInwardNormals() {
			n1 = v1 - v2;
			n1 = Vector2(-n1.y, n1.x);
			n1 = n1 * (1.0f / sqrtf(n1.x * n1.x + n1.y * n1.y));
			n2 = v2 - v3;
			n2 = Vector2(-n2.y, n2.x);
			n2 = n2 * (1.0f / sqrtf(n2.x * n2.x + n2.y * n2.y));
			n3 = v3 - v1;
			n3 = Vector2(-n3.y, n3.x);
			n3 = n3 * (1.0f / sqrtf(n3.x * n3.x + n3.y * n3.y));
		}

		bool drawAA(SamplingCallback cb, void *param) {
			const float PX_INSIDE = 1.0f / sqrtf(2.0f);
			const float PX_OUTSIDE = -1.0f / sqrtf(2.0f);
			const float BK_SIZE = 8;
			const float BK_INSIDE = sqrtf(BK_SIZE * BK_SIZE / 2.0f);
			const float BK_OUTSIDE = -sqrtf(BK_SIZE * BK_SIZE / 2.0f);
			float minx, miny, maxx, maxy;
			// Bounding rectangle
			minx = floorf(std::max(std::min(v1.x, std::min(v2.x, v3.x)), 0.0f));
			miny = floorf(std::max(std::min(v1.y, std::min(v2.y, v3.y)), 0.0f));
			maxx = ceilf(std::max(v1.x, std::max(v2.x, v3.x)));
			maxy = ceilf(std::max(v1.y, std::max(v2.y, v3.y)));
			// There's no reason to align the blocks to the viewport, instead we align them to the origin of the triangle bounds.
			minx = floorf(minx);
			miny = floorf(miny);
			//minx = (float)(((int)minx) & (~((int)BK_SIZE - 1))); // align to blocksize (we don't need to worry about blocks partially out of viewport)
			//miny = (float)(((int)miny) & (~((int)BK_SIZE - 1)));
			minx += 0.5;
			miny += 0.5; // sampling at texel centers!
			maxx += 0.5;
			maxy += 0.5;
			// Half-edge constants
			float C1 = n1.x * (-v1.x) + n1.y * (-v1.y);
			float C2 = n2.x * (-v2.x) + n2.y * (-v2.y);
			float C3 = n3.x * (-v3.x) + n3.y * (-v3.y);
			// Loop through blocks
			for (float y0 = miny; y0 <= maxy; y0 += BK_SIZE) {
				for (float x0 = minx; x0 <= maxx; x0 += BK_SIZE) {
					// Corners of block
					float xc = (x0 + (BK_SIZE - 1) / 2.0f);
					float yc = (y0 + (BK_SIZE - 1) / 2.0f);
					// Evaluate half-space functions
					float aC = C1 + n1.x * xc + n1.y * yc;
					float bC = C2 + n2.x * xc + n2.y * yc;
					float cC = C3 + n3.x * xc + n3.y * yc;
					// Skip block when outside an edge
					if ((aC <= BK_OUTSIDE) || (bC <= BK_OUTSIDE) || (cC <= BK_OUTSIDE)) continue;
					// Accept whole block when totally covered
					if ((aC >= BK_INSIDE) && (bC >= BK_INSIDE) && (cC >= BK_INSIDE)) {
						Vector3 texRow = t1 + dy * (y0 - v1.y) + dx * (x0 - v1.x);
						for (float y = y0; y < y0 + BK_SIZE; y++) {
							Vector3 tex = texRow;
							for (float x = x0; x < x0 + BK_SIZE; x++) {
								if (!cb(param, (int)x, (int)y, tex, dx, dy, 1.0f)) {
									return false;
								}
								tex += dx;
							}
							texRow += dy;
						}
					} else { // Partially covered block
						float CY1 = C1 + n1.x * x0 + n1.y * y0;
						float CY2 = C2 + n2.x * x0 + n2.y * y0;
						float CY3 = C3 + n3.x * x0 + n3.y * y0;
						Vector3 texRow = t1 + dy * (y0 - v1.y) + dx * (x0 - v1.x);
						for (float y = y0; y < y0 + BK_SIZE; y++) { // @@ This is not clipping to scissor rectangle correctly.
							float CX1 = CY1;
							float CX2 = CY2;
							float CX3 = CY3;
							Vector3 tex = texRow;
							for (float x = x0; x < x0 + BK_SIZE; x++) { // @@ This is not clipping to scissor rectangle correctly.
								Vector3 tex2 = t1 + dx * (x - v1.x) + dy * (y - v1.y);
								if (CX1 >= PX_INSIDE && CX2 >= PX_INSIDE && CX3 >= PX_INSIDE) {
									// pixel completely covered
									if (!cb(param, (int)x, (int)y, tex2, dx, dy, 1.0f)) {
										return false;
									}
								} else if ((CX1 >= PX_OUTSIDE) && (CX2 >= PX_OUTSIDE) && (CX3 >= PX_OUTSIDE)) {
									// triangle partially covers pixel. do clipping.
									ClippedTriangle ct(v1 - Vector2(x, y), v2 - Vector2(x, y), v3 - Vector2(x, y));
									ct.clipAABox(-0.5, -0.5, 0.5, 0.5);
									float area = ct.area();
									if (area > 0.0f) {
										if (!cb(param, (int)x, (int)y, tex2, dx, dy, 0.0f)) {
											return false;
										}
									}
								}
								CX1 += n1.x;
								CX2 += n2.x;
								CX3 += n3.x;
								tex += dx;
							}
							CY1 += n1.y;
							CY2 += n2.y;
							CY3 += n3.y;
							texRow += dy;
						}
					}
				}
			}
			return true;
		}

		Vector2 v1, v2, v3;
		Vector2 n1, n2, n3; // unit inward normals
		Vector3 t1, t2, t3;
		Vector3 dx, dy;
	};

	class ClippedTriangle {
	public:
		ClippedTriangle(const Vector2 &a, const Vector2 &b, const Vector2 &c) {
			m_numVertices = 3;
			m_activeVertexBuffer = 0;
			m_verticesA[0] = a;
			m_verticesA[1] = b;
			m_verticesA[2] = c;
			m_vertexBuffers[0] = m_verticesA;
			m_vertexBuffers[1] = m_verticesB;
		}

		void clipHorizontalPlane(float offset, float clipdirection) {
			Vector2 *v = m_vertexBuffers[m_activeVertexBuffer];
			m_activeVertexBuffer ^= 1;
			Vector2 *v2 = m_vertexBuffers[m_activeVertexBuffer];
			v[m_numVertices] = v[0];
			float dy2, dy1 = offset - v[0].y;
			int dy2in, dy1in = clipdirection * dy1 >= 0;
			uint32_t p = 0;
			for (uint32_t k = 0; k < m_numVertices; k++) {
				dy2 = offset - v[k + 1].y;
				dy2in = clipdirection * dy2 >= 0;
				if (dy1in) v2[p++] = v[k];
				if (dy1in + dy2in == 1) { // not both in/out
					float dx = v[k + 1].x - v[k].x;
					float dy = v[k + 1].y - v[k].y;
					v2[p++] = Vector2(v[k].x + dy1 * (dx / dy), offset);
				}
				dy1 = dy2;
				dy1in = dy2in;
			}
			m_numVertices = p;
		}

		void clipVerticalPlane(float offset, float clipdirection) {
			Vector2 *v = m_vertexBuffers[m_activeVertexBuffer];
			m_activeVertexBuffer ^= 1;
			Vector2 *v2 = m_vertexBuffers[m_activeVertexBuffer];
			v[m_numVertices] = v[0];
			float dx2, dx1 = offset - v[0].x;
			int dx2in, dx1in = clipdirection * dx1 >= 0;
			uint32_t p = 0;
			for (uint32_t k = 0; k < m_numVertices; k++) {
				dx2 = offset - v[k + 1].x;
				dx2in = clipdirection * dx2 >= 0;
				if (dx1in) v2[p++] = v[k];
				if (dx1in + dx2in == 1) { // not both in/out
					float dx = v[k + 1].x - v[k].x;
					float dy = v[k + 1].y - v[k].y;
					v2[p++] = Vector2(offset, v[k].y + dx1 * (dy / dx));
				}
				dx1 = dx2;
				dx1in = dx2in;
			}
			m_numVertices = p;
		}

		void computeAreaCentroid() {
			Vector2 *v = m_vertexBuffers[m_activeVertexBuffer];
			v[m_numVertices] = v[0];
			m_area = 0;
			float centroidx = 0, centroidy = 0;
			for (uint32_t k = 0; k < m_numVertices; k++) {
				// http://local.wasp.uwa.edu.au/~pbourke/geometry/polyarea/
				float f = v[k].x * v[k + 1].y - v[k + 1].x * v[k].y;
				m_area += f;
				centroidx += f * (v[k].x + v[k + 1].x);
				centroidy += f * (v[k].y + v[k + 1].y);
			}
			m_area = 0.5f * fabsf(m_area);
			if (m_area == 0) {
				m_centroid = Vector2(0.0f, 0.0f);
			} else {
				m_centroid = Vector2(centroidx / (6 * m_area), centroidy / (6 * m_area));
			}
		}

		void clipAABox(float x0, float y0, float x1, float y1) {
			clipVerticalPlane(x0, -1);
			clipHorizontalPlane(y0, -1);
			clipVerticalPlane(x1, 1);
			clipHorizontalPlane(y1, 1);
			computeAreaCentroid();
		}

		Vector2 centroid() {
			return m_centroid;
		}

		float area() {
			return m_area;
		}

	private:
		Vector2 m_verticesA[7 + 1];
		Vector2 m_verticesB[7 + 1];
		Vector2 *m_vertexBuffers[2];
		uint32_t m_numVertices;
		uint32_t m_activeVertexBuffer;
		float m_area;
		Vector2 m_centroid;
	};

	struct AtlasLookupTexel {
		uint16_t material_index;
		uint16_t x, y;
	};

	struct SetAtlasTexelArgs {
		Ref<Image> atlasData;
		Ref<Image> sourceTexture;
		Vector<AtlasLookupTexel> atlas_lookup;
		uint16_t material_index = 0;
		Vector2 source_uvs[3];
		uint32_t atlas_width = 0;
	};

	static bool setAtlasTexel(void *param, int x, int y, const Vector3 &bar, const Vector3 &, const Vector3 &, float);

	struct ModelVertex {
		Vector3 pos;
		Vector3 normal;
		Vector2 uv;
		// TODO (Ernest) Bones
		// TODO (Ernest) UV2
	};

	void _find_all_mesh_instances(Vector<MeshInstance *> &r_items, Node *p_current_node, const Node *p_owner);
	struct MergeState {
		Node *p_root;
		xatlas::Atlas *atlas;
		Vector<MeshInstance *> &r_mesh_items;
		Array &vertex_to_material;
		const Vector<Vector<Vector2>> uvs;
		const Vector<Vector<ModelVertex> > &model_vertices;
		String p_name;
		const xatlas::PackOptions &pack_options;
		Vector<AtlasLookupTexel> &atlas_lookup;
		Vector<Ref<Material> > &material_cache;
		Map<String, Ref<Image> > texture_atlas;
	};
	Ref<Image> dilate(Ref<Image> source_image);
public:

	Node *merge(Node *p_root, Node *p_original_root);

	void _generate_texture_atlas(MergeState& state, String texture_type);

	Ref<Image> _get_source_texture(MergeState &state, Map<uint16_t, Ref<Image> > &image_cache, const xatlas::Chart &chart, Ref<StandardMaterial3D> &material, String texture_type);

	void _generate_atlas(const int32_t p_num_meshes, Vector<Vector<Vector2>> &r_uvs, xatlas::Atlas *atlas, const Vector<MeshInstance *> &r_meshes, Array vertex_to_material, const Vector<Ref<Material> > material_cache,
			xatlas::PackOptions &pack_options);
	void scale_uvs_by_texture_dimension(const Vector<MeshInstance *> &original_mesh_items, Vector<MeshInstance *> &mesh_items, Vector<Vector<Vector2>> &uv_groups, Array &r_vertex_to_material, Vector<Vector<ModelVertex> > &r_model_vertices);
	void map_vertex_to_material(const Vector<MeshInstance *> mesh_items, Array &vertex_to_material, Vector<Ref<Material> > &material_cache);
	Node *_output(MergeState &state);
};
#endif
