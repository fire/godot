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

		AtlasLookupTexel &lookup = args->atlas_lookup.write[x + y * args->atlas_width];
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
			for (int32_t i = 0; i < array_mesh->get_surface_count(); i++) {
				Array array = array_mesh->surface_get_arrays(i);
				Array bones = array[ArrayMesh::ARRAY_BONES];
				if (bones.size()) {
					has_bones |= true;
				}
				if (array_mesh->get_blend_shape_count()) {
					has_blends |= true;
				}
			}
			if (!has_blends && !has_bones) {
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
	generate_atlas(num_surfaces, uv_groups, atlas, mesh_items, vertex_to_material, material_cache, pack_options);
	atlas_lookup.resize(atlas->width * atlas->height);
	p_root = output(p_root, atlas, mesh_items, vertex_to_material, uv_groups, model_vertices, p_root->get_name(), pack_options, atlas_lookup, material_cache);

	xatlas::Destroy(atlas);
	return p_root;
}

void MeshMergeMaterialRepack::generate_atlas(const int32_t p_num_meshes, PoolVector<PoolVector2Array> &r_uvs, xatlas::Atlas *atlas, Vector<MeshInstance *> &r_meshes, Array vertex_to_material, const Vector<Ref<Material> > material_cache,
		xatlas::PackOptions &pack_options) {

	int32_t mesh_first_index = 0;
	uint32_t mesh_count = 0;
	for (int32_t i = 0; i < r_meshes.size(); i++) {
		for (int32_t j = 0; j < r_meshes[i]->get_mesh()->get_surface_count(); j++) {
			// Handle blend shapes?
			Array mesh = r_meshes[i]->get_mesh()->surface_get_arrays(j);
			if (mesh.empty()) {
				r_meshes.remove(i);
				continue;
			}
			Array vertices = mesh[ArrayMesh::ARRAY_VERTEX];
			if (vertices.empty()) {
				r_meshes.remove(i);
				continue;
			}
			Array indices = mesh[ArrayMesh::ARRAY_INDEX];
			if (indices.empty()) {
				r_meshes.remove(i);
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
				if (material.is_valid()) {
					if (material_cache.find(material) != -1) {
						materials.write[k] = material_cache.find(material);
					}
				} else {
					materials.write[k] = 0;
				}
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
	pack_options.padding = 4;
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

void MeshMergeMaterialRepack::map_vertex_to_material(Vector<MeshInstance *> mesh_items, Array &vertex_to_material, Vector<Ref<Material> > &material_cache) {
	for (int32_t i = 0; i < mesh_items.size(); i++) {
		for (int32_t j = 0; j < mesh_items[i]->get_mesh()->get_surface_count(); j++) {
			Array mesh = mesh_items[i]->get_mesh()->surface_get_arrays(j);
			if (mesh.empty()) {
				mesh_items.remove(i);
				continue;
			}
			PoolVector3Array indices = mesh[ArrayMesh::ARRAY_INDEX];
			if (!indices.size()) {
				mesh_items.remove(i);
				continue;
			}
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

Node *MeshMergeMaterialRepack::output(Node *p_root, xatlas::Atlas *atlas, Vector<MeshInstance *> &r_mesh_items, Array &vertex_to_material, const PoolVector<PoolVector2Array> uvs, const PoolVector<PoolVector<ModelVertex> > &model_vertices, String p_name, const xatlas::PackOptions &pack_options, Vector<AtlasLookupTexel> &atlas_lookup, Vector<Ref<Material> > &material_cache) {
	MeshMergeMaterialRepack::TextureData texture_data;
	Ref<Image> atlas_img_albedo;
	atlas_img_albedo.instance();
	ERR_FAIL_COND_V(atlas->width <= 0 && atlas->height <= 0, p_root);
	atlas_img_albedo->create(atlas->width, atlas->height, true, Image::FORMAT_RGBA8);
	// Rasterize chart triangles.
	Map<uint16_t, Ref<Image> > image_cache;
	for (uint32_t i = 0; i < atlas->meshCount; i++) {
		const xatlas::Mesh &mesh = atlas->meshes[i];
		for (uint32_t j = 0; j < mesh.chartCount; j++) {
			const xatlas::Chart &chart = mesh.chartArray[j];
			Ref<SpatialMaterial> material;
			Ref<Image> img;
			img.instance();
			Map<uint16_t, Ref<Image> >::Element *E = image_cache.find(chart.material);
			if (E) {
				img = E->get();
			} else {
				if (chart.material >= material_cache.size()) {
					continue;
				}
				material = material_cache.get(chart.material);
				if (material.is_null()) {
					continue;
				}
				Ref<Texture> tex = material->get_texture(SpatialMaterial::TEXTURE_ALBEDO);
				if (tex.is_null()) {
					img->create(1, 1, true, Image::FORMAT_RGBA8);
					img->fill(material->get_albedo());
				} else {
					img = tex->get_data();
					if (img->is_compressed()) {
						img->decompress();
					}
					img->lock();
					for (int32_t y = 0; y < img->get_height(); y++) {
						for (int32_t x = 0; x < img->get_width(); x++) {
							Color c = img->get_pixel(x, y);
							img->set_pixel(x, y, c * material->get_albedo());
						}
					}
				}
				img->unlock();
				image_cache.insert(chart.material, img);
			}
			ERR_EXPLAIN("Float textures are not supported yet");
			ERR_CONTINUE(Image::get_format_pixel_size(img->get_format()) > 4);
			if (img->is_compressed()) {
				img->decompress();
				Ref<ImageTexture> image_texture;
				image_texture.instance();
				image_texture->create_from_image(img);
				material = material_cache.get(chart.material);
				material->set_texture(SpatialMaterial::TEXTURE_ALBEDO, image_texture);
			}
			img->convert(Image::FORMAT_RGBA8);
			SetAtlasTexelArgs args;
			args.sourceTexture = img;
			args.atlasData = atlas_img_albedo;
			args.sourceTexture->lock();
			args.atlasData->lock();
			args.atlas_lookup = atlas_lookup;
			args.material_index = (uint16_t)chart.material;
			for (uint32_t k = 0; k < chart.faceCount; k++) {
				Vector2 v[3];
				for (uint32_t l = 0; l < 3; l++) {
					const uint32_t index = mesh.indexArray[chart.faceArray[k] * 3 + l];
					const xatlas::Vertex &vertex = mesh.vertexArray[index];
					v[l] = Vector2(vertex.uv[0], vertex.uv[1]);
					args.source_uvs[l].x = uvs[i][vertex.xref].x / img->get_width();
					args.source_uvs[l].y = uvs[i][vertex.xref].y / img->get_height();
				}
				Triangle tri(v[0], v[1], v[2], Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1));

				tri.drawAA(setAtlasTexel, &args);
			}
			args.atlasData->unlock();
			args.sourceTexture->unlock();
		}
	}

	if (pack_options.padding > 0) {
		// Run a dilate filter on the atlas texture to fill in padding around charts so bilinear filtering doesn't sample empty texels.
		// Sample from the source texture(s).
		Ref<Image> temp_atlas_img_albedo;
		temp_atlas_img_albedo.instance();
		temp_atlas_img_albedo->create(atlas->width, atlas->height, true, Image::FORMAT_RGBA8, atlas_img_albedo->get_data());
		temp_atlas_img_albedo->fill(Color(0.0f, 0.0f, 0.0f, 1.0f));
		PoolVector<AtlasLookupTexel> temp_atlas_lookup;
		temp_atlas_lookup.resize(atlas_lookup.size());
		const int sampleXOffsets[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
		const int sampleYOffsets[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
		// Only 2 levels of the 4 pixel padding is needed to preserve filtering
		// TODO use https://blog.ostermiller.org/dilate-and-erode
		for (uint32_t i = 0; i < 2; i++) {
			memcpy(temp_atlas_img_albedo->get_data().write().ptr(), atlas_img_albedo->get_data().read().ptr(), atlas_img_albedo->get_data().size() * sizeof(uint8_t));
			memcpy(temp_atlas_lookup.write().ptr(), atlas_lookup.ptr(), atlas_lookup.size() * sizeof(AtlasLookupTexel));
			for (uint32_t y = 0; y < atlas->height; y++) {
				for (uint32_t x = 0; x < atlas->width; x++) {
					// Try to sample directly from the source texture.
					// Need to find source texel position by checking surrounding texels in the atlas.
					bool foundSample = false;
					for (uint32_t si = 0; si < 8; si++) {
						if (x < 0 || y < 0 || x >= (int)atlas->width || x >= (int)atlas->height) {
							continue; // Sample position is outside of atlas texture.
						}
						const int sx = (int)x + sampleXOffsets[si];
						const int sy = (int)y + sampleYOffsets[si];
						if (sx < 0 || sy < 0 || sx >= (int)atlas->width || sy >= (int)atlas->height) {
							continue; // Sample position is outside of atlas texture.
						}
						const AtlasLookupTexel &lookup = temp_atlas_lookup[sx + sy * (int)atlas->width];
						if (lookup.material_index == 0) {
							continue;
						}
						// This atlas texel has a corresponding position for the source texel.
						// Subtract the sample offset to get the source position.
						Ref<Image> img;
						Map<uint16_t, Ref<Image> >::Element *E = image_cache.find(lookup.material_index);
						if (E) {
							img = E->get();
						} else {
							ERR_CONTINUE(lookup.material_index >= material_cache.size());
							Ref<SpatialMaterial> mat = material_cache[lookup.material_index];
							if (mat.is_null()) {
								continue;
							}
							img = mat->get_texture(SpatialMaterial::TEXTURE_ALBEDO)->get_data();
							if (img.is_null()) {
								continue;
							}
							if (img->is_compressed()) {
								img->decompress();
							}
							image_cache.insert(lookup.material_index, img);
						}

						int ssx = (int)lookup.x - sampleXOffsets[si];
						int ssy = (int)lookup.y - sampleYOffsets[si] * -1; // need to flip y?

						// Valid sample.
						img->lock();
						atlas_img_albedo->lock();
						if (ssx < 0 || ssy < 0 || ssx >= img->get_width() || ssy >= img->get_height()) {
							continue; // Sample position is outside of source texture.
						}
						if (x < 0 || y < 0 || x >= atlas_img_albedo->get_width() || y >= atlas_img_albedo->get_height()) {
							continue; // Sample position is outside of source texture.
						}
						atlas_img_albedo->set_pixel(x, y, img->get_pixel(ssx, ssy));
						atlas_img_albedo->unlock();
						img->unlock();
						AtlasLookupTexel temp_lookup = temp_atlas_lookup.get(x + y * (int)atlas->width);
						temp_lookup.x = (uint16_t)ssx;
						temp_lookup.y = (uint16_t)ssy;
						temp_lookup.material_index = lookup.material_index;
						temp_atlas_lookup.write()[x + y * (int)atlas->width] = temp_lookup;
						atlas_lookup.write[x + y * (int)atlas->width] = temp_lookup;
						foundSample = true;
						break;
					}
					if (foundSample) {
						continue;
					}
					// Sample up to 8 surrounding texels in the source texture, average their color and assign it to this texel.
					float rgb_sum[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, n = 0;
					for (uint32_t si = 0; si < 8; si++) {
						const int sx = (int)x + sampleXOffsets[si];
						const int sy = (int)y + sampleYOffsets[si];
						if (sx < 0 || sy < 0 || sx >= (int)atlas->width || sy >= (int)atlas->height) {
							continue; // Sample position is outside of atlas texture.
						}
						const AtlasLookupTexel &lookup = temp_atlas_lookup[sx + sy * (int)atlas->width];
						Ref<Image> img;
						Map<uint16_t, Ref<Image> >::Element *E = image_cache.find(lookup.material_index);
						if (E) {
							img = E->get();
						} else {
							Ref<SpatialMaterial> mat = material_cache[lookup.material_index];
							if (mat.is_null()) {
								continue;
							}
							img = mat->get_texture(SpatialMaterial::TEXTURE_ALBEDO)->get_data();
							if (img.is_null()) {
								continue;
							}
							if (img->is_compressed()) {
								img->decompress();
							}
							image_cache.insert(lookup.material_index, img);
						}
						int ssx = (int)lookup.x + sampleXOffsets[si];
						int ssy = (int)lookup.y + sampleYOffsets[si];

						if (ssx < 0 || ssy < 0 || ssx >= img->get_width() || ssy >= img->get_height()) {
							continue; // Sample position is outside of source texture.
						}

						img->lock();
						Color color = img->get_pixel(ssx, ssy);
						img->unlock();
						rgb_sum[0] += color.r;
						rgb_sum[1] += color.g;
						rgb_sum[2] += color.b;
						rgb_sum[3] += color.a;
						n++;
					}
					if (n != 0) {
						const float invn = 1.0f / (float)n;
						Color color;
						color.r = rgb_sum[0] * invn;
						color.g = rgb_sum[1] * invn;
						color.b = rgb_sum[2] * invn;
						color.a = rgb_sum[3] * invn;
						temp_atlas_img_albedo->lock();
						temp_atlas_img_albedo->set_pixel(x, y, color);
						temp_atlas_img_albedo->unlock();
						continue;
					}
					// Sample up to 8 surrounding texels in the atlas texture, average their color and assign it to this texel.
					rgb_sum[0] = rgb_sum[1] = rgb_sum[2] = 0.0f;
					rgb_sum[3] = 1.0f;
					n = 0;
					for (uint32_t si = 0; si < 8; si++) {
						const int sx = (int)x + sampleXOffsets[si];
						const int sy = (int)y + sampleYOffsets[si];
						if (sx < 0 || sy < 0 || sx >= (int)atlas->width || sy >= (int)atlas->height) {
							continue; // Sample position is outside of atlas texture.
						}
						atlas_img_albedo->lock();
						Color color = atlas_img_albedo->get_pixel(sx, sy);
						atlas_img_albedo->unlock();
						rgb_sum[0] += color.r;
						rgb_sum[1] += color.g;
						rgb_sum[2] += color.b;
						rgb_sum[3] += color.a;
						n++;
					}
					if (n != 0) {
						const float invn = 1.0f / (float)n;
						Color color;
						color.r = rgb_sum[0] * invn;
						color.g = rgb_sum[1] * invn;
						color.b = rgb_sum[2] * invn;
						color.a = rgb_sum[3] * invn;

						atlas_img_albedo->lock();
						atlas_img_albedo->set_pixel(x, y, color);
						atlas_img_albedo->unlock();
					}
				}
			}
		}
	}

	for (int32_t i = 0; i < r_mesh_items.size(); i++) {
		if (r_mesh_items[i]->get_parent()) {
			r_mesh_items[i]->get_parent()->remove_child(r_mesh_items[i]);
		}
	}
	Ref<SurfaceTool> st_all;
	st_all.instance();
	st_all->begin(Mesh::PRIMITIVE_TRIANGLES);
	for (uint32_t i = 0; i < atlas->meshCount; i++) {
		Ref<SurfaceTool> st;
		st.instance();
		st->begin(Mesh::PRIMITIVE_TRIANGLES);
		const xatlas::Mesh &mesh = atlas->meshes[i];
		for (uint32_t v = 0; v < mesh.vertexCount; v++) {
			const xatlas::Vertex vertex = mesh.vertexArray[v];
			const ModelVertex &sourceVertex = model_vertices[i][vertex.xref];
			// TODO (Ernest) UV2
			st->add_uv(Vector2(vertex.uv[0] / atlas->width, vertex.uv[1] / atlas->height));
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
	if (atlas->width != 0 || atlas->height != 0) {
		Ref<ImageTexture> texture;
		texture.instance();
		atlas_img_albedo->generate_mipmaps();
		atlas_img_albedo->compress();
		texture->create_from_image(atlas_img_albedo);
		mat->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);

		if (atlas_img_albedo->detect_alpha()) {
			mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
			mat->set_depth_draw_mode(SpatialMaterial::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
		}
	}
	MeshInstance *mi = memnew(MeshInstance);
	Ref<ArrayMesh> array_mesh = st_all->commit();
	mi->set_mesh(array_mesh);
	mi->set_name(p_name + "Merged");
	array_mesh->surface_set_material(0, mat);
	p_root->add_child(mi);
	mi->set_owner(p_root);
	return p_root;
}
