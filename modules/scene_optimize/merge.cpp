/*************************************************************************/
/*  mesh_merge_material_repack.cpp                                       */
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

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/os/os.h"
#include "optimize.h"
#include "scene/resources/mesh_data_tool.h"
#include "scene/resources/surface_tool.h"

#include "merge.h"

bool MeshMergeMaterialRepack::setAtlasTexel(void *param, int x, int y, const Vector3 &bar, const Vector3 &, const Vector3 &, float) {
	SetAtlasTexelArgs *args = (SetAtlasTexelArgs *)param;
	if (args->sourceTexture.is_valid()) {
		// Interpolate source UVs using barycentrics.
		const Vector2 sourceUv = args->source_uvs[0] * bar.x + args->source_uvs[1] * bar.y + args->source_uvs[2] * bar.z;
		// Keep coordinates in range of texture dimensions.
		int _width = args->sourceTexture->get_width();
		float sx = sourceUv.x * _width;
		while (sx < 0) {
			sx += _width;
		}
		if ((int32_t)sx >= _width) {
			sx = Math::fmod(sx, _width);
		}
		int _height = args->sourceTexture->get_height();
		float sy = sourceUv.y * _height;
		while (sy < 0) {
			sy += _height;
		}
		if ((int32_t)sy >= _height) {
			sy = Math::fmod(sy, _height);
		}
		const Color color = args->sourceTexture->get_pixel(sx, sy);
		args->atlasData->set_pixel(x, y, color);

		AtlasLookupTexel &lookup = args->atlas_lookup.write[x * y + args->atlas_width];
		lookup.material_index = args->material_index;
		lookup.x = (uint16_t)sx;
		lookup.y = (uint16_t)sy;
		return true;
	}
	return false;
}

void MeshMergeMaterialRepack::_find_all_mesh_instances(Vector<MeshInstance *> &r_items, Node *p_current_node, const Node *p_owner) {
	MeshInstance *mi = Object::cast_to<MeshInstance>(p_current_node);
	if (mi) {
		Ref<ArrayMesh> array_mesh = mi->get_mesh();
		if (!array_mesh.is_null()) {
			bool has_blends = false;
			bool has_bones = false;
			bool has_transparency = false;
			bool has_emission = false;
			for (int32_t i = 0; i < array_mesh->get_surface_count(); i++) {
				Array array = array_mesh->surface_get_arrays(i);
				Array bones = array[ArrayMesh::ARRAY_BONES];
				if (bones.size()) {
					has_bones |= true;
				}
				if (array_mesh->get_blend_shape_count()) {
					has_blends |= true;
				}
				Ref<SpatialMaterial> spatial_mat = array_mesh->surface_get_material(i);
				if (spatial_mat.is_valid()) {
					Ref<Image> img = spatial_mat->get_texture(SpatialMaterial::TEXTURE_ALBEDO);
					if (spatial_mat->get_albedo().a != 1.0f || (img.is_valid() && img->detect_alpha() != Image::ALPHA_NONE)) {
						has_transparency |= true;
					}
					if (spatial_mat->get_feature(SpatialMaterial::FEATURE_EMISSION)) {
						has_emission |= true;
					}
				}
			}
			if (!has_blends && !has_bones && !has_transparency && !has_emission) {
				r_items.push_back(mi);
			}
		}
	}
	for (int32_t i = 0; i < p_current_node->get_child_count(); i++) {
		_find_all_mesh_instances(r_items, p_current_node->get_child(i), p_owner);
	}
}

Node *MeshMergeMaterialRepack::merge(Node *p_root, Node *p_original_root) {
	Vector<MeshInstance *> mesh_items;
	_find_all_mesh_instances(mesh_items, p_root, p_root);
	Vector<MeshInstance *> original_mesh_items;
	_find_all_mesh_instances(original_mesh_items, p_original_root, p_original_root);
	if (!original_mesh_items.size()) {
		return p_root;
	}
	Array vertex_to_material;
	Vector<Ref<Material> > material_cache;
	Ref<Material> empty_material;
	material_cache.push_back(empty_material);
	map_vertex_to_material(mesh_items, vertex_to_material, material_cache);

	PoolVector<PoolVector2Array> uv_groups;
	PoolVector<PoolVector<ModelVertex> > model_vertices;
	scale_uvs_by_texture_dimension(original_mesh_items, mesh_items, uv_groups, vertex_to_material, model_vertices);

	xatlas::SetPrint(printf, true);
	xatlas::Atlas *atlas = xatlas::Create();

	int32_t num_surfaces = 0;

	for (int32_t i = 0; i < mesh_items.size(); i++) {
		for (int32_t j = 0; j < mesh_items[i]->get_mesh()->get_surface_count(); j++) {
			Array mesh = mesh_items[i]->get_mesh()->surface_get_arrays(j);
			if (mesh.empty()) {
				continue;
			}
			PoolVector3Array vertices = mesh[ArrayMesh::ARRAY_VERTEX];
			if (!vertices.size()) {
				continue;
			}
			num_surfaces++;
		}
	}
	xatlas::PackOptions pack_options;
	Vector<AtlasLookupTexel> atlas_lookup;
	_generate_atlas(num_surfaces, uv_groups, atlas, mesh_items, vertex_to_material, material_cache, pack_options);
	atlas_lookup.resize(atlas->width * atlas->height);
	Map<String, Ref<Image> > texture_atlas;
	MergeState state = { p_root, atlas, mesh_items, vertex_to_material, uv_groups, model_vertices, p_root->get_name(), pack_options, atlas_lookup, material_cache, texture_atlas };

	print_line("Generating albedo texture atlas.");
	_generate_texture_atlas(state, "albedo");
	//print_line("Generating emission texture atlas.");
	//_generate_texture_atlas(state, "emission");
	print_line("Generating normal texture atlas.");
	_generate_texture_atlas(state, "normal");
	print_line("Generating orm texture atlas.");
	_generate_texture_atlas(state, "orm");
	ERR_FAIL_COND_V(state.atlas->width <= 0 && state.atlas->height <= 0, state.p_root);
	p_root = _output(state);

	xatlas::Destroy(atlas);
	return p_root;
}

void MeshMergeMaterialRepack::_generate_texture_atlas(MergeState &state, String texture_type) {
	Ref<Image> atlas_img;
	atlas_img.instance();
	atlas_img->create(state.atlas->width, state.atlas->height, false, Image::FORMAT_RGBA8);
	// Rasterize chart triangles.
	Map<uint16_t, Ref<Image> > image_cache;
	for (uint32_t i = 0; i < state.atlas->meshCount; i++) {
		const xatlas::Mesh &mesh = state.atlas->meshes[i];
		print_line("  mesh atlas " + itos(i));
		for (uint32_t j = 0; j < mesh.chartCount; j++) {
			const xatlas::Chart &chart = mesh.chartArray[j];
			Ref<SpatialMaterial> material;
			Ref<Image> img = _get_source_texture(state, image_cache, chart, material, texture_type);
			ERR_EXPLAIN("Float textures are not supported yet");
			ERR_CONTINUE(Image::get_format_pixel_size(img->get_format()) > 4);
			Ref<ImageTexture> image_texture;
			image_texture.instance();
			image_texture->create_from_image(img);
			img->convert(Image::FORMAT_RGBA8);
			SetAtlasTexelArgs args;
			args.sourceTexture = img;
			args.atlasData = atlas_img;
			args.sourceTexture->lock();
			args.atlasData->lock();
			args.atlas_lookup = state.atlas_lookup;
			args.material_index = (uint16_t)chart.material;
			for (uint32_t k = 0; k < chart.faceCount; k++) {
				Vector2 v[3];
				for (uint32_t l = 0; l < 3; l++) {
					const uint32_t index = mesh.indexArray[chart.faceArray[k] * 3 + l];
					const xatlas::Vertex &vertex = mesh.vertexArray[index];
					v[l] = Vector2(vertex.uv[0], vertex.uv[1]);
					args.source_uvs[l].x = state.uvs[i][vertex.xref].x / img->get_width();
					args.source_uvs[l].y = state.uvs[i][vertex.xref].y / img->get_height();
				}
				Triangle tri(v[0], v[1], v[2], Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1));

				tri.drawAA(setAtlasTexel, &args);
			}
			args.atlasData->unlock();
			args.sourceTexture->unlock();
		}
	}
	atlas_img->generate_mipmaps();
	state.texture_atlas.insert(texture_type, atlas_img);
}

Ref<Image> MeshMergeMaterialRepack::_get_source_texture(MergeState &state, Map<uint16_t, Ref<Image> > &image_cache, const xatlas::Chart &chart, Ref<SpatialMaterial> &material, String texture_type) {
	Ref<Image> img;
	img.instance();
	img->create(32, 32, false, Image::FORMAT_RGBA8);
	Map<uint16_t, Ref<Image> >::Element *E = image_cache.find(chart.material);
	if (E) {
		img = E->get();
	} else {
		material = state.material_cache.get(chart.material);
		Ref<Texture> tex;
		if (texture_type == "orm") {
			img.instance();
			float width = state.atlas->width;
			float height = state.atlas->height;
			Ref<Image> ao_img = material->get_texture(SpatialMaterial::TEXTURE_AMBIENT_OCCLUSION);
			if (ao_img.is_valid() && !ao_img->empty()) {
				width = ao_img->get_width();
				height = ao_img->get_height();
			}
			Ref<Image> metallic_img = material->get_texture(SpatialMaterial::TEXTURE_METALLIC);
			if (metallic_img.is_valid() && !metallic_img->empty()) {
				width = metallic_img->get_width();
				height = metallic_img->get_height();
			}
			Ref<Image> roughness_img = material->get_texture(SpatialMaterial::TEXTURE_ROUGHNESS);
			if (roughness_img.is_valid() && !roughness_img->empty()) {
				width = roughness_img->get_width();
				height = roughness_img->get_height();
			}
			if (ao_img.is_valid() && !ao_img->empty()) {
				ao_img->resize(width, height, Image::INTERPOLATE_LANCZOS);
			}
			if (metallic_img.is_valid() && !metallic_img->empty()) {
				metallic_img->resize(width, height, Image::INTERPOLATE_LANCZOS);
			}
			if (roughness_img.is_valid() && !roughness_img->empty()) {
				roughness_img->resize(width, height, Image::INTERPOLATE_LANCZOS);
			}
			img->create(width, height, false, Image::FORMAT_RGBA8);
			img->lock();

			for (int32_t y = 0; y < img->get_height(); y++) {
				for (int32_t x = 0; x < img->get_width(); x++) {
					Color c = img->get_pixel(x, y);
					Color orm;
					if (ao_img.is_valid() && !ao_img->empty()) {
						ao_img->lock();
						if (material->get_ao_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_RED) {
							orm.r = ao_img->get_pixel(x, y).r;
						} else if (material->get_ao_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_GREEN) {
							orm.r = ao_img->get_pixel(x, y).g;
						} else if (material->get_ao_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_BLUE) {
							orm.r = ao_img->get_pixel(x, y).b;
						} else if (material->get_ao_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_ALPHA) {
							orm.r = ao_img->get_pixel(x, y).a;
						} else if (material->get_ao_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_GRAYSCALE) {
							orm.r = ao_img->get_pixel(x, y).r;
						}
						ao_img->unlock();
					}
					if (roughness_img.is_valid() && !roughness_img->empty()) {
						roughness_img->lock();
						if (material->get_ao_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_RED) {
							orm.g = roughness_img->get_pixel(x, y).r;
						} else if (material->get_roughness_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_GREEN) {
							orm.g = roughness_img->get_pixel(x, y).g;
						} else if (material->get_roughness_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_BLUE) {
							orm.g = roughness_img->get_pixel(x, y).b;
						} else if (material->get_roughness_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_ALPHA) {
							orm.g = roughness_img->get_pixel(x, y).a;
						} else if (material->get_roughness_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_GRAYSCALE) {
							orm.g = roughness_img->get_pixel(x, y).r;
						}
						roughness_img->unlock();
						orm.g *= material->get_roughness();
					} else {
						orm.g = material->get_roughness();
					}
					if (metallic_img.is_valid() && !metallic_img->empty()) {
						metallic_img->lock();
						if (material->get_ao_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_RED) {
							orm.b = metallic_img->get_pixel(x, y).r;
						} else if (material->get_metallic_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_GREEN) {
							orm.b = metallic_img->get_pixel(x, y).g;
						} else if (material->get_metallic_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_BLUE) {
							orm.b = metallic_img->get_pixel(x, y).b;
						} else if (material->get_metallic_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_ALPHA) {
							orm.b = metallic_img->get_pixel(x, y).a;
						} else if (material->get_metallic_texture_channel() == SpatialMaterial::TEXTURE_CHANNEL_GRAYSCALE) {
							orm.b = metallic_img->get_pixel(x, y).r;
						}
						metallic_img->unlock();
						orm.b *= material->get_metallic();
					} else {
						orm.b = material->get_metallic();
					}
					img->set_pixel(x, y, orm);
				}
			}
		} else if (texture_type == "albedo") {
			tex = material->get_texture(SpatialMaterial::TEXTURE_ALBEDO);
			if (tex.is_valid()) {
				img = tex->get_data();
				if (!img->empty()) {
					if (img->is_compressed()) {
						img->decompress();
					}
				}
				if (img.is_valid() && !img->empty()) {
					img->lock();
					for (int32_t y = 0; y < img->get_height(); y++) {
						for (int32_t x = 0; x < img->get_width(); x++) {
							Color c = img->get_pixel(x, y);
							img->set_pixel(x, y, c * material->get_albedo());
						}
					}
					img->unlock();
				}
			} else {
				img->fill(material->get_albedo());
			}
		} else if (texture_type == "emission") {
			//tex = material->get_texture(SpatialMaterial::TEXTURE_EMISSION);
			//if (tex.is_valid()) {
			//	img = tex->get_data();
			//	if (!img->empty()) {
			//		if (img->is_compressed()) {
			//			img->decompress();
			//		}
			//	}
			//	if (img.is_valid() && !img->empty()) {
			//		img->lock();
			//		for (int32_t y = 0; y < img->get_height(); y++) {
			//			for (int32_t x = 0; x < img->get_width(); x++) {
			//				Color c = img->get_pixel(x, y);
			//				img->set_pixel(x, y, c + material->get_emission());
			//			}
			//		}
			//		img->unlock();
			//	}
			//} else {
			//	img->fill(material->get_emission());
			//}
		} else if (texture_type == "normal") {
			tex = material->get_texture(SpatialMaterial::TEXTURE_NORMAL);
			if (tex.is_valid()) {
				img = tex->get_data();
				if (!img->empty()) {
					if (img->is_compressed()) {
						img->decompress();
					}
				}
			}
		}
	}
	img->unlock();
	image_cache.insert(chart.material, img);
	return img;
}

void MeshMergeMaterialRepack::_generate_atlas(const int32_t p_num_meshes, PoolVector<PoolVector2Array> &r_uvs, xatlas::Atlas *atlas, const Vector<MeshInstance *> &r_meshes, Array vertex_to_material, const Vector<Ref<Material> > material_cache,
		xatlas::PackOptions &pack_options) {

	int32_t mesh_first_index = 0;
	uint32_t mesh_count = 0;
	for (int32_t i = 0; i < r_meshes.size(); i++) {
		for (int32_t j = 0; j < r_meshes[i]->get_mesh()->get_surface_count(); j++) {
			// Handle blend shapes?
			Array mesh = r_meshes[i]->get_mesh()->surface_get_arrays(j);
			if (mesh.empty()) {
				continue;
			}
			Array vertices = mesh[ArrayMesh::ARRAY_VERTEX];
			if (vertices.empty()) {
				continue;
			}
			Array indices = mesh[ArrayMesh::ARRAY_INDEX];
			if (indices.empty()) {
				continue;
			}
			xatlas::UvMeshDecl meshDecl;
			meshDecl.vertexCount = r_uvs[mesh_count].size();
			meshDecl.vertexUvData = r_uvs[mesh_count].read().ptr();
			meshDecl.vertexStride = sizeof(Vector2);
			PoolIntArray mesh_indices = mesh[Mesh::ARRAY_INDEX];
			Vector<uint32_t> indexes;
			indexes.resize(mesh_indices.size());
			Vector<uint32_t> materials;
			materials.resize(mesh_indices.size());
			const Array materials_array = vertex_to_material[mesh_count];
			for (int32_t k = 0; k < mesh_indices.size(); k++) {
				indexes.write[k] = mesh_indices[k];
				ERR_FAIL_COND(k >= materials_array.size());
				Ref<Material> material = materials_array[k];
				if (!material.is_valid()) {
					continue;
				}
				if (material_cache.find(material) == -1) {
					continue;
				}
				materials.write[k] = material_cache.find(material);
			}
			meshDecl.indexCount = indexes.size();
			meshDecl.indexData = indexes.ptr();
			meshDecl.indexFormat = xatlas::IndexFormat::UInt32;
			meshDecl.indexOffset = 0;
			meshDecl.faceMaterialData = materials.ptr();
			meshDecl.rotateCharts = false;
			xatlas::AddMeshError::Enum error = xatlas::AddUvMesh(atlas, meshDecl);
			if (error != xatlas::AddMeshError::Success) {
				OS::get_singleton()->print("Error adding mesh %d: %s\n", i, xatlas::StringForEnum(error));
				ERR_CONTINUE(error != xatlas::AddMeshError::Success);
			}
			mesh_first_index += vertices.size();
			mesh_count++;
		}
	}
	pack_options.texelsPerUnit = 1.0f;
	pack_options.maxChartSize = 4096;
	pack_options.blockAlign = true;
	xatlas::PackCharts(atlas, pack_options);
}

void MeshMergeMaterialRepack::scale_uvs_by_texture_dimension(Vector<MeshInstance *> &original_mesh_items, Vector<MeshInstance *> &mesh_items, PoolVector<PoolVector2Array> &uv_groups, Array &r_vertex_to_material, PoolVector<PoolVector<ModelVertex> > &r_model_vertices) {
	for (int32_t i = 0; i < mesh_items.size(); i++) {
		for (int32_t j = 0; j < mesh_items[i]->get_mesh()->get_surface_count(); j++) {
			r_model_vertices.push_back(PoolVector<ModelVertex>());
		}
	}
	uint32_t mesh_count = 0;
	for (int32_t i = 0; i < mesh_items.size(); i++) {
		for (int32_t j = 0; j < mesh_items[i]->get_mesh()->get_surface_count(); j++) {

			Array mesh = mesh_items[i]->get_mesh()->surface_get_arrays(j);
			if (mesh.empty()) {
				mesh_items.remove(i);
				continue;
			}
			Array vertices = mesh[ArrayMesh::ARRAY_VERTEX];
			if (vertices.size() == 0) {
				mesh_items.remove(i);
				continue;
			}
			PoolVector3Array vertex_arr = mesh[Mesh::ARRAY_VERTEX];
			PoolVector3Array normal_arr = mesh[Mesh::ARRAY_NORMAL];
			PoolVector2Array uv_arr = mesh[Mesh::ARRAY_TEX_UV];
			PoolIntArray index_arr = mesh[Mesh::ARRAY_INDEX];
			Transform xform = original_mesh_items[i]->get_global_transform();

			PoolVector<ModelVertex> model_vertices;
			model_vertices.resize(vertex_arr.size());
			for (int32_t k = 0; k < vertex_arr.size(); k++) {
				ModelVertex vertex;
				vertex.pos = xform.xform(vertex_arr[k]);
				if (normal_arr.size()) {
					vertex.normal = normal_arr[k];
				}
				if (uv_arr.size()) {
					vertex.uv = uv_arr[k];
				}
				model_vertices.write()[k] = vertex;
			}
			r_model_vertices.write()[mesh_count] = model_vertices;
			mesh_count++;
		}
	}
	mesh_count = 0;
	for (int32_t i = 0; i < mesh_items.size(); i++) {
		for (int32_t j = 0; j < mesh_items[i]->get_mesh()->get_surface_count(); j++) {
			Array mesh = mesh_items[i]->get_mesh()->surface_get_arrays(j);
			if (mesh.empty()) {
				mesh_items.remove(i);
				continue;
			}
			PoolVector3Array vertices = mesh[ArrayMesh::ARRAY_VERTEX];
			if (vertices.size() == 0) {
				mesh_items.remove(i);
				continue;
			}
			PoolVector2Array uvs;
			uvs.resize(vertices.size());
			for (uint32_t k = 0; k < vertices.size(); k++) {
				Ref<SpatialMaterial> empty_material;
				empty_material.instance();
				if (mesh_count >= r_vertex_to_material.size()) {
					break;
				}
				Array vertex_to_material = r_vertex_to_material[mesh_count];
				if (!vertex_to_material.size()) {
					continue;
				}
				if (k >= vertex_to_material.size()) {
					continue;
				}
				const Ref<Material> material = vertex_to_material.get(k);
				if (material.is_null()) {
					break;
				}
				if (!Object::cast_to<SpatialMaterial>(*material)) {
					continue;
				}
				ERR_CONTINUE(material->get_class_name() != empty_material->get_class_name());
				const Ref<Texture> tex = Object::cast_to<SpatialMaterial>(*material)->get_texture(SpatialMaterial::TextureParam::TEXTURE_ALBEDO);
				uvs.write()[k] = r_model_vertices.read()[mesh_count].read()[k].uv;
				if (tex.is_valid()) {
					uvs.write()[k].x *= (float)tex->get_width();
					uvs.write()[k].y *= (float)tex->get_height();
				}
			}
			uv_groups.push_back(uvs);
			mesh_count++;
		}
	}
}

void MeshMergeMaterialRepack::map_vertex_to_material(const Vector<MeshInstance *> mesh_items, Array &vertex_to_material, Vector<Ref<Material> > &material_cache) {
	for (int32_t i = 0; i < mesh_items.size(); i++) {
		for (int32_t j = 0; j < mesh_items[i]->get_mesh()->get_surface_count(); j++) {
			Array mesh = mesh_items[i]->get_mesh()->surface_get_arrays(j);
			PoolVector3Array indices = mesh[ArrayMesh::ARRAY_INDEX];
			Array materials;
			materials.resize(indices.size());
			Ref<Material> mat = mesh_items[i]->get_mesh()->surface_get_material(j);
			if (mesh_items[i]->get_surface_material(j).is_valid()) {
				mat = mesh_items[i]->get_surface_material(j);
			}
			if (material_cache.find(mat) == -1) {
				material_cache.push_back(mat);
			}
			for (int32_t k = 0; k < indices.size(); k++) {
				if (mat.is_valid()) {
					materials[k] = mat;
				} else {
					Ref<SpatialMaterial> new_mat;
					new_mat.instance();
					materials[k] = new_mat;
				}
			}
			vertex_to_material.push_back(materials);
		}
	}
}

Node *MeshMergeMaterialRepack::_output(MergeState &state) {
	MeshMergeMaterialRepack::TextureData texture_data;
	for (int32_t i = 0; i < state.r_mesh_items.size(); i++) {
		if (state.r_mesh_items[i]->get_parent()) {
			state.r_mesh_items[i]->get_parent()->remove_child(state.r_mesh_items[i]);
		}
	}
	Ref<SurfaceTool> st_all;
	st_all.instance();
	st_all->begin(Mesh::PRIMITIVE_TRIANGLES);
	for (uint32_t i = 0; i < state.atlas->meshCount; i++) {
		Ref<SurfaceTool> st;
		st.instance();
		st->begin(Mesh::PRIMITIVE_TRIANGLES);
		const xatlas::Mesh &mesh = state.atlas->meshes[i];
		for (uint32_t v = 0; v < mesh.vertexCount; v++) {
			const xatlas::Vertex vertex = mesh.vertexArray[v];
			const ModelVertex &sourceVertex = state.model_vertices[i][vertex.xref];
			st->add_uv(Vector2(vertex.uv[0] / state.atlas->width, vertex.uv[1] / state.atlas->height));
			st->add_normal(Vector3(sourceVertex.normal.x, sourceVertex.normal.y, sourceVertex.normal.z));
			st->add_vertex(Vector3(sourceVertex.pos.x, sourceVertex.pos.y, sourceVertex.pos.z));
		}

		for (uint32_t f = 0; f < mesh.indexCount; f++) {
			const uint32_t index = mesh.indexArray[f];
			st->add_index(index);
		}
		Ref<ArrayMesh> array_mesh = st->commit();
		st_all->append_from(array_mesh, 0, Transform());
	}
	Ref<SpatialMaterial> mat;
	mat.instance();
	mat->set_name("Atlas");
	if (state.atlas->width != 0 || state.atlas->height != 0) {
		Map<String, Ref<Image> >::Element *A = state.texture_atlas.find("albedo");
		if (A && !A->get()->empty()) {
			Ref<ImageTexture> texture;
			texture.instance();
			texture->create_from_image(A->get());
			texture->set_storage(ImageTexture::Storage::STORAGE_COMPRESS_LOSSY);
			texture->set_lossy_storage_quality(0.75);
			mat->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);
		}
		Map<String, Ref<Image> >::Element *E = state.texture_atlas.find("emission");
		if (E && !E->get()->empty()) {
			Ref<ImageTexture> texture;
			texture.instance();
			texture->create_from_image(E->get());
			texture->set_storage(ImageTexture::Storage::STORAGE_COMPRESS_LOSSY);
			texture->set_lossy_storage_quality(0.75);
			mat->set_feature(SpatialMaterial::FEATURE_EMISSION, true);
			mat->set_texture(SpatialMaterial::TEXTURE_EMISSION, texture);
		}
		Map<String, Ref<Image> >::Element *N = state.texture_atlas.find("normal");
		if (N && !N->get()->empty()) {
			bool has_normals = false;
			N->get()->lock();
			for (int32_t y = 0; y < N->get()->get_height(); y++) {
				for (int32_t x = 0; x < N->get()->get_width(); x++) {
					Color texel = N->get()->get_pixel(x, y);
					if (texel != Color(0.0f, 0.0f, 0.0f, 0.0f)) {
						has_normals = has_normals || true;
					}
				}
			}
			N->get()->unlock();
			if (has_normals) {
				Ref<ImageTexture> texture;
				texture.instance();
				texture->create_from_image(N->get());
				texture->set_storage(ImageTexture::Storage::STORAGE_COMPRESS_LOSSY);
				texture->set_lossy_storage_quality(0.75);
				mat->set_feature(SpatialMaterial::FEATURE_NORMAL_MAPPING, true);
				mat->set_texture(SpatialMaterial::TEXTURE_NORMAL, texture);
			}
		}
		Map<String, Ref<Image> >::Element *ORM = state.texture_atlas.find("orm");
		if (ORM && !ORM->get()->empty()) {
			Ref<ImageTexture> texture;
			texture.instance();
			texture->create_from_image(ORM->get());
			texture->set_storage(ImageTexture::Storage::STORAGE_COMPRESS_LOSSY);
			texture->set_lossy_storage_quality(0.75);
			mat->set_ao_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_RED);
			mat->set_feature(SpatialMaterial::FEATURE_AMBIENT_OCCLUSION, true);
			mat->set_texture(SpatialMaterial::TEXTURE_AMBIENT_OCCLUSION, texture);
			mat->set_roughness_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_GREEN);
			mat->set_texture(SpatialMaterial::TEXTURE_ROUGHNESS, texture);
			mat->set_metallic_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_BLUE);
			mat->set_texture(SpatialMaterial::TEXTURE_METALLIC, texture);
		}
	}
	MeshInstance *mi = memnew(MeshInstance);
	Ref<ArrayMesh> array_mesh = st_all->commit();
	mi->set_mesh(array_mesh);
	mi->set_name(state.p_name + "Merged");
	array_mesh->surface_set_material(0, mat);
	state.p_root->add_child(mi);
	mi->set_owner(state.p_root);
	return state.p_root;
}
