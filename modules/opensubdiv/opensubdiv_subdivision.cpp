/*************************************************************************/
/*  opensubdiv_subdivision.cpp                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "opensubdiv_subdivision.h"

#define M_PI 3.14159265358979323846
#include "far/primvarRefiner.h"
#include "far/topologyDescriptor.h"

using namespace OpenSubdiv;

typedef Far::TopologyDescriptor Descriptor;

struct Vertex {
	void Clear() { x = y = z = 0; }
	void AddWithWeight(Vertex const &src, float weight) {
		x += weight * src.x;
		y += weight * src.y;
		z += weight * src.z;
	}
	float x, y, z;
};

OpenSubdivMeshSubdivision::OpenSubdivMeshSubdivision() {
	subdiv_mesh = VisualServer::get_singleton()->mesh_create();
	subdiv_vertex_count = 0;
	refiner = NULL;
}

OpenSubdivMeshSubdivision ::~OpenSubdivMeshSubdivision() {
	VisualServer::get_singleton()->free(subdiv_mesh);
	subdiv_mesh = RID();
	source_mesh = RID();

	surface_data.clear();
	subdiv_vertex_count = 0;

	if (refiner) {
		delete refiner;
		refiner = NULL;
	}
}

void OpenSubdivMeshSubdivision::update_subdivision(Ref<Mesh> p_mesh, int p_level) {
	VisualServer::get_singleton()->mesh_clear(subdiv_mesh);

	surface_data.clear();
	subdiv_vertex_count = 0;

	if (refiner) {
		delete refiner;
		refiner = NULL;
	}

	ERR_FAIL_COND(p_level <= 0);

	ERR_FAIL_COND(p_mesh.is_null());
	source_mesh = p_mesh->get_rid();
	ERR_FAIL_COND(!source_mesh.is_valid());

	if (p_mesh->get_blend_shape_count() > 0) {
		ERR_PRINT("Blend shapes are not supported for mesh subdivision.");
	}

	VisualServer *visual_server = VisualServer::get_singleton();

	int surface_count = p_mesh->get_surface_count();
	surface_data.resize(surface_count);

	int subdiv_index_count = 0;

	PoolVector3Array subdiv_vertex_array;
	PoolIntArray subdiv_index_array;

	int subdiv_face_count = 0;
	Vector<int> face_to_surface_index_map;

	Map<Vector3, int> vertex_map;

	// Gather all vertices and faces from surfaces
	for (int surface_index = 0; surface_index < surface_count; ++surface_index) {
		SurfaceData &surface = surface_data.write[surface_index];

		ERR_FAIL_COND(Mesh::PRIMITIVE_TRIANGLES != p_mesh->surface_get_primitive_type(surface_index));

		Array mesh_arrays = p_mesh->surface_get_arrays(surface_index);
		PoolVector3Array vertex_array = mesh_arrays[Mesh::ARRAY_VERTEX];
		PoolIntArray index_array = mesh_arrays[Mesh::ARRAY_INDEX];

		int index_count = index_array.size();

		// Remove duplicated vertices to link faces properly
		{
			int vertex_source_count = vertex_array.size();

			surface.mesh_to_subdiv_index_map.resize(vertex_source_count);
			subdiv_vertex_array.resize(subdiv_vertex_count + vertex_source_count);

			PoolVector3Array::Read vertex_array_read = vertex_array.read();
			PoolVector3Array::Write subdiv_vertex_array_write = subdiv_vertex_array.write();
			int vertex_index_out = 0;
			for (int vertex_index = 0; vertex_index < vertex_source_count; ++vertex_index) {
				const Vector3 &vertex = vertex_array_read[vertex_index];
				Map<Vector3, int>::Element *found_vertex = vertex_map.find(vertex);
				if (found_vertex) {
					surface.mesh_to_subdiv_index_map.write[vertex_index] = found_vertex->value();
				} else {
					int subdiv_vertex_index = subdiv_vertex_count + vertex_index_out;
					vertex_map[vertex] = subdiv_vertex_index;
					surface.mesh_to_subdiv_index_map.write[vertex_index] = subdiv_vertex_index;
					subdiv_vertex_array_write[subdiv_vertex_index] = vertex;
					++vertex_index_out;
				}
			}

			subdiv_vertex_count += vertex_index_out;
		}
		subdiv_vertex_array.resize(subdiv_vertex_count);

		// Add vertex indices
		{
			subdiv_index_array.resize(subdiv_index_count + index_count);

			PoolIntArray::Read index_array_read = index_array.read();
			PoolIntArray::Write subdiv_index_array_write = subdiv_index_array.write();
			for (int index = 0; index < index_count; ++index) {
				int subdiv_index = subdiv_index_count + index;
				subdiv_index_array_write[subdiv_index] = surface.mesh_to_subdiv_index_map[index_array_read[index]];
			}

			subdiv_index_count += index_count;

			int face_count = index_count / 3;

			face_to_surface_index_map.resize(subdiv_face_count + face_count);
			for (int face_index = 0; face_index < face_count; ++face_index) {
				face_to_surface_index_map.write[subdiv_face_count + face_index] = surface_index;
			}

			subdiv_face_count += face_count;
		}
	}

	// Generate subdivision data
	Vector<PoolIntArray> index_arrays_out;
	index_arrays_out.resize(surface_count);
	{
		// Create per-face vertex count
		Vector<int> subdiv_face_vertex_count;
		{
			subdiv_face_vertex_count.resize(subdiv_face_count);
			for (int face_index = 0; face_index < subdiv_face_count; ++face_index) {
				subdiv_face_vertex_count.write[face_index] = 3;
			}
		}

		PoolIntArray::Read subdiv_index_array_read = subdiv_index_array.read();

		Far::TopologyDescriptor desc;
		desc.numVertices = subdiv_vertex_count;
		desc.numFaces = subdiv_face_count;
		desc.numVertsPerFace = subdiv_face_vertex_count.ptr();
		desc.vertIndicesPerFace = subdiv_index_array_read.ptr();

		// Create topology refiner
		Sdc::SchemeType type = OpenSubdiv::Sdc::SCHEME_LOOP;

		Sdc::Options options;
		options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
		options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
		options.SetCreasingMethod(Sdc::Options::CREASE_UNIFORM);

		Far::TopologyRefinerFactory<Descriptor>::Options create_options(type, options);

		refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc, create_options);
		ERR_FAIL_COND(!refiner);

		Far::TopologyRefiner::UniformOptions refine_options(p_level);
		refiner->RefineUniform(refine_options);

		subdiv_vertex_count = refiner->GetNumVerticesTotal();

		// Create subdivision vertices
		{
			subdiv_vertex_array.resize(subdiv_vertex_count);

			PoolVector3Array::Write subdiv_vertex_array_write = subdiv_vertex_array.write();

			// Interpolate vertex primvar data
			Far::PrimvarRefiner primvar_refiner(*refiner);

			Vertex *src = (Vertex *)subdiv_vertex_array_write.ptr();
			for (int level = 0; level < p_level; ++level) {
				Vertex *dst = src + refiner->GetLevel(level).GetNumVertices();
				primvar_refiner.Interpolate(level + 1, src, dst);
				src = dst;
			}
		}

		// Create subdivision faces
		{
			Far::TopologyLevel const &last_level = refiner->GetLevel(p_level);
			int face_count_out = last_level.GetNumFaces();
			int index_count_out = face_count_out * 3;

			int vertex_index_offset = subdiv_vertex_count - last_level.GetNumVertices();

			for (int face_index = 0; face_index < face_count_out; ++face_index) {
				int parent_face_index = last_level.GetFaceParentFace(face_index);
				for (int level_index = p_level - 1; level_index > 0; --level_index) {
					Far::TopologyLevel const &prev_level = refiner->GetLevel(level_index);
					parent_face_index = prev_level.GetFaceParentFace(parent_face_index);
				}

				int surface_index = face_to_surface_index_map[parent_face_index];
				PoolIntArray &index_array_out = index_arrays_out.write[surface_index];

				Far::ConstIndexArray face_vertices = last_level.GetFaceVertices(face_index);
				ERR_FAIL_COND(face_vertices.size() != 3);

				index_array_out.push_back(vertex_index_offset + face_vertices[0]);
				index_array_out.push_back(vertex_index_offset + face_vertices[1]);
				index_array_out.push_back(vertex_index_offset + face_vertices[2]);
			}
		}
	}

	// Create all subdivision surfaces
	for (int surface_index = 0; surface_index < surface_count; ++surface_index) {
		const PoolIntArray &index_array_out = index_arrays_out[surface_index];

		Array subdiv_mesh_arrays;
		subdiv_mesh_arrays.resize(Mesh::ARRAY_MAX);
		subdiv_mesh_arrays[Mesh::ARRAY_VERTEX] = subdiv_vertex_array;
		subdiv_mesh_arrays[Mesh::ARRAY_INDEX] = index_array_out;

		uint32_t surface_format = p_mesh->surface_get_format(surface_index);
		surface_format &= ~Mesh::ARRAY_COMPRESS_VERTEX;
		surface_format |= Mesh::ARRAY_FLAG_USE_DYNAMIC_UPDATE;

		// TODO: use mesh_add_surface to share vertex array
		visual_server->mesh_add_surface_from_arrays(subdiv_mesh, VisualServer::PRIMITIVE_TRIANGLES, subdiv_mesh_arrays, Array(), surface_format);

		Ref<Material> material = p_mesh->surface_get_material(surface_index);
		visual_server->mesh_surface_set_material(subdiv_mesh, surface_index, material.is_null() ? RID() : material->get_rid());
	}
}

void OpenSubdivMeshSubdivision::update_skinning(RID p_skeleton) {
	// TODO: change to static assert
	ERR_FAIL_COND(4 != Mesh::ARRAY_WEIGHTS_SIZE);
	ERR_FAIL_COND(sizeof(Vertex) != sizeof(Vector3));

	int surface_count = surface_data.size();
	if (surface_count == 0) {
		return;
	}

	ERR_FAIL_COND(!source_mesh.is_valid());
	ERR_FAIL_COND(!subdiv_mesh.is_valid());

	VisualServer *visual_server = VisualServer::get_singleton();

	PoolByteArray subdiv_buffer = visual_server->mesh_surface_get_array(subdiv_mesh, 0);
	ERR_FAIL_COND(subdiv_buffer.size() != subdiv_vertex_count * sizeof(Vector3));
	PoolByteArray::Write subdiv_buffer_write = subdiv_buffer.write();

	// Apply skinning
	for (int surface_index = 0; surface_index < surface_count; ++surface_index) {
		const SurfaceData &surface = surface_data[surface_index];

		uint32_t format = visual_server->mesh_surface_get_format(source_mesh, surface_index);

		ERR_CONTINUE(0 == (format & Mesh::ARRAY_FORMAT_BONES));
		ERR_CONTINUE(0 == (format & Mesh::ARRAY_FORMAT_WEIGHTS));

		const int vertex_count = visual_server->mesh_surface_get_array_len(source_mesh, surface_index);
		const int index_count = visual_server->mesh_surface_get_array_index_len(source_mesh, surface_index);

		uint32_t array_offsets[Mesh::ARRAY_MAX];
		uint32_t stride = visual_server->mesh_surface_make_offsets_from_format(format, vertex_count, index_count, array_offsets);
		uint32_t offset_vertices = array_offsets[Mesh::ARRAY_VERTEX];
		uint32_t offset_bones = array_offsets[Mesh::ARRAY_BONES];
		uint32_t offset_weights = array_offsets[Mesh::ARRAY_WEIGHTS];

		PoolByteArray buffer = visual_server->mesh_surface_get_array(source_mesh, surface_index);
		PoolByteArray::Read buffer_read = buffer.read();

		for (int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
			int subdiv_vertex_index = surface.mesh_to_subdiv_index_map[vertex_index];

			float bone_weight[4];
			if (format & Mesh::ARRAY_COMPRESS_WEIGHTS) {
				const uint16_t *weight_ptr = (const uint16_t *)(buffer_read.ptr() + offset_weights + (vertex_index * stride));
				bone_weight[0] = (weight_ptr[0] / (float)0xFFFF);
				bone_weight[1] = (weight_ptr[1] / (float)0xFFFF);
				bone_weight[2] = (weight_ptr[2] / (float)0xFFFF);
				bone_weight[3] = (weight_ptr[3] / (float)0xFFFF);
			} else {
				const float *weight_ptr = (const float *)(buffer_read.ptr() + offset_weights + (vertex_index * stride));
				bone_weight[0] = weight_ptr[0];
				bone_weight[1] = weight_ptr[1];
				bone_weight[2] = weight_ptr[2];
				bone_weight[3] = weight_ptr[3];
			}

			int bone_id[4];
			if (format & Mesh::ARRAY_FLAG_USE_16_BIT_BONES) {
				const uint16_t *bones_ptr = (const uint16_t *)(buffer_read.ptr() + offset_bones + (vertex_index * stride));
				bone_id[0] = bones_ptr[0];
				bone_id[1] = bones_ptr[1];
				bone_id[2] = bones_ptr[2];
				bone_id[3] = bones_ptr[3];
			} else {
				const uint8_t *bones_ptr = buffer_read.ptr() + offset_bones + (vertex_index * stride);
				bone_id[0] = bones_ptr[0];
				bone_id[1] = bones_ptr[1];
				bone_id[2] = bones_ptr[2];
				bone_id[3] = bones_ptr[3];
			}

			Transform bone_transform[4] = {
				visual_server->skeleton_bone_get_transform(p_skeleton, bone_id[0]),
				visual_server->skeleton_bone_get_transform(p_skeleton, bone_id[1]),
				visual_server->skeleton_bone_get_transform(p_skeleton, bone_id[2]),
				visual_server->skeleton_bone_get_transform(p_skeleton, bone_id[3]),
			};

			Transform transform;
			transform.origin =
					bone_weight[0] * bone_transform[0].origin +
					bone_weight[1] * bone_transform[1].origin +
					bone_weight[2] * bone_transform[2].origin +
					bone_weight[3] * bone_transform[3].origin;

			transform.basis =
					bone_transform[0].basis * bone_weight[0] +
					bone_transform[1].basis * bone_weight[1] +
					bone_transform[2].basis * bone_weight[2] +
					bone_transform[3].basis * bone_weight[3];

			const Vector3 &vertex = (const Vector3 &)buffer_read[vertex_index * stride + offset_vertices];
			Vector3 &vertex_out = (Vector3 &)subdiv_buffer_write[subdiv_vertex_index * sizeof(Vector3)];
			vertex_out = transform.xform(vertex);
		}
	}

	// Update vertex arrays for each surface
	for (int surface_index = 0; surface_index < surface_count; ++surface_index) {
		const SurfaceData &surface = surface_data[surface_index];

		if (surface_index == 0) {
			// Main surface, interpolate vertex primvar data
			Vertex *src = (Vertex *)subdiv_buffer_write.ptr();
			Far::PrimvarRefiner primvar_refiner(*refiner);
			int level_count = refiner->GetMaxLevel();
			for (int level = 0; level < level_count; ++level) {
				Vertex *dst = src + refiner->GetLevel(level).GetNumVertices();
				primvar_refiner.Interpolate(level + 1, src, dst);
				src = dst;
			}
		}

		// All surfaces use the same vertex data
		visual_server->mesh_surface_update_region(subdiv_mesh, surface_index, 0, subdiv_buffer);
	}
}

MeshSubdivision *OpenSubdivSubdivisionSystem::create_mesh_subdivision(Ref<Mesh> p_mesh, int p_level) {
	OpenSubdivMeshSubdivision *mesh_subdivision = memnew(OpenSubdivMeshSubdivision);
	mesh_subdivision->update_subdivision(p_mesh, p_level);

	return mesh_subdivision;
}

void OpenSubdivSubdivisionSystem::destroy_mesh_subdivision(MeshSubdivision *p_mesh_subdivision) {
	memdelete(p_mesh_subdivision);
}
