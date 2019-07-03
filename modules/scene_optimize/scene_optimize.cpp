/*************************************************************************/
/*  scene_optimize.cpp                                                   */
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

#include "scene_optimize.h"

#include "core/object.h"
#include "core/project_settings.h"
#include "core/vector.h"
#include "mesh_merge_material_pack.h"
#include "modules/csg/csg_shape.h"
#include "modules/gridmap/grid_map.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/spatial.h"
#include "scene/gui/check_box.h"
#include "scene/resources/mesh_data_tool.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/surface_tool.h"
#include "thirdparty/meshoptimizer/src/meshoptimizer.h"

#ifdef TOOLS_ENABLED
void SceneOptimize::scene_optimize(const String p_file, Node *p_root_node) {
	Ref<PackedScene> packed_scene;
	packed_scene.instance();
	packed_scene->pack(p_root_node);
	Node *root = packed_scene->instance();
	ERR_FAIL_COND(p_root_node == NULL);
	Vector<MeshInstance *> mesh_items;
	_find_all_mesh_instances(mesh_items, root, root);

	Vector<CSGShape *> csg_items;
	_find_all_csg_roots(csg_items, root, root);

	Vector<GridMap *> grid_map_items;
	_find_all_gridmaps(grid_map_items, root, root);

	Vector<MeshInfo> meshes;
	for (int32_t i = 0; i < mesh_items.size(); i++) {
		MeshInfo mesh_info;
		mesh_info.mesh = mesh_items[i]->get_mesh();
		mesh_info.transform = mesh_items[i]->get_transform();
		mesh_info.name = mesh_items[i]->get_name();
		mesh_info.original_node = mesh_items[i];
		mesh_info.skeleton_path = mesh_items[i]->get_skeleton_path();
		meshes.push_back(mesh_info);
	}
	for (int32_t i = 0; i < csg_items.size(); i++) {
		MeshInfo mesh_info;
		mesh_info.mesh = csg_items[i]->get_calculated_mesh();
		mesh_info.transform = csg_items[i]->get_transform();
		mesh_info.name = csg_items[i]->get_name();
		mesh_info.original_node = csg_items[i];
		meshes.push_back(mesh_info);
	}
	for (int32_t i = 0; i < grid_map_items.size(); i++) {
		Array cells = grid_map_items[i]->get_used_cells();
		for (int32_t k = 0; k < cells.size(); k++) {
			Vector3 cell_location = cells[k];
			int32_t cell = grid_map_items[i]->get_cell_item(cell_location.x, cell_location.y, cell_location.z);
			MeshInfo mesh_info;
			mesh_info.mesh = grid_map_items[i]->get_mesh_library()->get_item_mesh(cell);
			Transform cell_xform;
			cell_xform.basis.set_orthogonal_index(grid_map_items[i]->get_cell_item_orientation(cell_location.x, cell_location.y, cell_location.z));
			cell_xform.basis.scale(Vector3(grid_map_items[i]->get_cell_scale(), grid_map_items[i]->get_cell_scale(), grid_map_items[i]->get_cell_scale()));
			cell_xform.set_origin(grid_map_items[i]->map_to_world(cell_location.x, cell_location.y, cell_location.z));
			mesh_info.transform = cell_xform * grid_map_items[i]->get_transform();
			mesh_info.name = grid_map_items[i]->get_mesh_library()->get_item_name(cell);
			mesh_info.original_node = grid_map_items[i];
			meshes.push_back(mesh_info);
		}
	}
	const size_t lod_count = 5;
	// const size_t kCacheSize = 16;
	struct Vertex {
		float px, py, pz;
	};

	// Ref<MeshMergeMaterialRepack> repack;
	// repack.instance();
	// repack->pack(meshes, p_root_node->get_name());

	for (int32_t i = 0; i < meshes.size(); i++) {
		Ref<Mesh> mesh = meshes[i].mesh;
		if (mesh->get_blend_shape_count()) {
			// Don't lod blend shapes.
			MeshInstance *mi = memnew(MeshInstance);
			mi->set_mesh(mesh);
			mi->set_skeleton_path(meshes[i].skeleton_path);
			mi->set_name(meshes[i].name);
			meshes[i].original_node->get_parent()->add_child(mi);
			mi->set_owner(root);
			continue;
		}
		for (int32_t j = 0; j < mesh->get_surface_count(); j++) {
			// double start = OS::get_singleton()->get_ticks_msec();

			Ref<SurfaceTool> st;
			st.instance();
			st->create_from(mesh, j);
			st->index();
			const Array mesh_array = st->commit_to_arrays();
			PoolVector<Vector3> vertexes = mesh_array[Mesh::ARRAY_VERTEX];
			// https://github.com/zeux/meshoptimizer/blob/bce99a4bfdc7bbc72479e1d71c4083329d306347/demo/main.cpp#L414
			// generate 4 LOD levels (1-4), with each subsequent LOD using 70% triangles
			// note that each LOD uses the same (shared) vertex buffer
			PoolVector<PoolVector<uint32_t> > lods;
			lods.resize(lod_count);
			PoolVector<PoolVector<uint32_t> >::Write w = lods.write();
			PoolVector<uint32_t> unsigned_indices;
			{
				PoolVector<int32_t> indices = mesh_array[Mesh::ARRAY_INDEX];
				unsigned_indices.resize(indices.size());
				for (int32_t o = 0; o < indices.size(); o++) {
					unsigned_indices.write()[o] = indices.read()[o];
				}
			}
			w[0] = unsigned_indices;
			PoolVector<Vertex> meshopt_vertices;
			meshopt_vertices.resize(vertexes.size());
			for (int32_t k = 0; k < vertexes.size(); k++) {
				Vertex meshopt_vertex;
				Vector3 vertex = vertexes.read()[k];
				meshopt_vertex.px = vertex.x;
				meshopt_vertex.py = vertex.y;
				meshopt_vertex.pz = vertex.z;
				meshopt_vertices.write()[k] = meshopt_vertex;
			}

			for (size_t l = 1; l < lod_count; ++l) {
				PoolVector<PoolVector<uint32_t> >::Write w = lods.write();
				PoolVector<uint32_t> &lod = w[l];

				float threshold = powf(0.7f, float(l));
				size_t target_index_count = size_t(unsigned_indices.size() * threshold) / 3 * 3;
				float target_error = 1e-2f;

				// we can simplify all the way from base level or from the last result
				// simplifying from the base level sometimes produces better results, but simplifying from last level is faster
				// simplify from the base level
				PoolVector<uint32_t> &source = w[0];

				if (source.size() < target_index_count)
					target_index_count = source.size();

				lod.resize(source.size());
				lod.resize(meshopt_simplify(&lod.write()[0], &source.write()[0], source.size(), &meshopt_vertices.write()[0].px, meshopt_vertices.size(), sizeof(Vertex), target_index_count, target_error));
			}

			// double middle = OS::get_singleton()->get_ticks_msec();

			// optimize each individual LOD for vertex cache & overdraw
			for (size_t m = 1; m < lod_count; m++) {
				PoolVector<uint32_t> &lod = lods.write()[m];

				meshopt_optimizeVertexCache(&lod.write()[0], &lod.write()[0], lod.size(), meshopt_vertices.size());
				meshopt_optimizeOverdraw(&lod.write()[0], &lod.write()[0], lod.size(), &meshopt_vertices.write()[0].px, meshopt_vertices.size(), sizeof(Vertex), 1.0f);
			}

			// TODO (Ernest)
			// // concatenate all LODs into one IB
			// // note: the order of concatenation is important - since we optimize the entire IB for vertex fetch,
			// // putting coarse LODs first makes sure that the vertex range referenced by them is as small as possible
			// // some GPUs process the entire range referenced by the index buffer region so doing this optimizes the vertex transform
			// // cost for coarse LODs
			// // this order also produces much better vertex fetch cache coherency for coarse LODs (since they're essentially optimized first)
			// // somewhat surprisingly, the vertex fetch cache coherency for fine LODs doesn't seem to suffer that much.
			size_t lod_index_offsets[lod_count] = {};
			size_t lod_index_counts[lod_count] = {};
			size_t total_index_count = 0;

			for (int n = lod_count - 1; n >= 0; --n) {
				lod_index_offsets[n] = total_index_count;
				lod_index_counts[n] = lods[n].size();

				total_index_count += lods[n].size();
			}

			for (int32_t r = 0; r < lods.size(); r++) {
				Array current_mesh = mesh_array.duplicate(true);
				PoolIntArray indexes = current_mesh[Mesh::ARRAY_INDEX].duplicate(true);
				size_t size = indexes.size();
				indexes.resize(0);
				indexes.resize(size);
				for (int32_t p = 0; p < lods[r].size(); p++) {
					indexes.write()[p] = lods[r][p];
				}
				current_mesh[Mesh::ARRAY_INDEX] = indexes;
				{
					st->clear();
					st->begin(Mesh::PRIMITIVE_TRIANGLES);
					Ref<ArrayMesh> array_mesh;
					array_mesh.instance();
					array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, current_mesh);
					st->create_from(array_mesh, 0);
					st->index();
				}
				Ref<ArrayMesh> final_mesh = st->commit();
				if (mesh->surface_get_material(j).is_valid()) {
					final_mesh->surface_set_material(0, mesh->surface_get_material(j)->duplicate(true));
				}
				MeshInstance *mi = memnew(MeshInstance);
				mi->set_mesh(final_mesh);
				mi->set_skeleton_path(meshes[i].skeleton_path);
				mi->set_name(String(meshes[i].name) + itos(j) + "Lod" + itos(r));
				meshes[i].original_node->get_parent()->add_child(mi);
				mi->set_owner(root);
			}
		}
	}

	for (int32_t i = 0; i < meshes.size(); i++) {
		Node *node = meshes[i].original_node;
		node->get_parent()->remove_child(node);
	}

	PackedScene *scene = memnew(PackedScene);
	scene->pack(root);
	ResourceSaver::save(p_file, scene);
}

void SceneOptimize::_find_all_mesh_instances(Vector<MeshInstance *> &r_items, Node *p_current_node, const Node *p_owner) {
	MeshInstance *mi = Object::cast_to<MeshInstance>(p_current_node);
	if (mi != NULL) {
		r_items.push_back(mi);
	}
	for (int32_t i = 0; i < p_current_node->get_child_count(); i++) {
		_find_all_mesh_instances(r_items, p_current_node->get_child(i), p_owner);
	}
}

void SceneOptimize::_find_all_gridmaps(Vector<GridMap *> &r_items, Node *p_current_node, const Node *p_owner) {
	GridMap *gridmap = Object::cast_to<GridMap>(p_current_node);
	if (gridmap != NULL) {
		r_items.push_back(gridmap);
		return;
	}
	for (int32_t i = 0; i < p_current_node->get_child_count(); i++) {
		_find_all_gridmaps(r_items, p_current_node->get_child(i), p_owner);
	}
}

void SceneOptimize::_find_all_csg_roots(Vector<CSGShape *> &r_items, Node *p_current_node, const Node *p_owner) {
	CSGShape *csg = Object::cast_to<CSGShape>(p_current_node);
	if (csg != NULL && csg->is_root_shape()) {
		r_items.push_back(csg);
		return;
	}
	for (int32_t i = 0; i < p_current_node->get_child_count(); i++) {
		_find_all_csg_roots(r_items, p_current_node->get_child(i), p_owner);
	}
}

#endif

void SceneOptimizePlugin::optimize(Variant p_user_data) {
	file_export_lib = memnew(EditorFileDialog);
	file_export_lib->set_title(TTR("Export Library"));
	file_export_lib->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	file_export_lib->connect("file_selected", this, "_dialog_action");
	file_export_lib_merge = memnew(CheckBox);
	file_export_lib_merge->set_text(TTR("Merge With Existing"));
	file_export_lib_merge->set_pressed(false);
	file_export_lib->get_vbox()->add_child(file_export_lib_merge);
	editor->get_gui_base()->add_child(file_export_lib);
	List<String> extensions;
	extensions.push_back("tscn");
	extensions.push_back("scn");
	file_export_lib->clear_filters();
	for (int i = 0; i < extensions.size(); i++) {
		file_export_lib->add_filter("*." + extensions[i] + " ; " + extensions[i].to_upper());
	}
	file_export_lib->popup_centered_ratio();
	file_export_lib->set_title(TTR("Optimize Scene"));
}

void SceneOptimizePlugin::_dialog_action(String p_file) {
	Node *node = editor->get_tree()->get_edited_scene_root();
	if (!node) {
		editor->show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
		return;
	}
	if (FileAccess::exists(p_file) && file_export_lib_merge->is_pressed()) {
		Ref<PackedScene> scene = ResourceLoader::load(p_file, "PackedScene");
		if (scene.is_null()) {
			editor->show_accept(TTR("Can't load scene for merging!"), TTR("OK"));
			return;
		} else {
			node->add_child(scene->instance());
		}
	}
	scene_optimize->scene_optimize(p_file, node);
	EditorFileSystem::get_singleton()->scan_changes();
	file_export_lib->queue_delete();
	file_export_lib_merge->queue_delete();
}
void SceneOptimizePlugin::_bind_methods() {
	ClassDB::bind_method("_dialog_action", &SceneOptimizePlugin::_dialog_action);
	ClassDB::bind_method(D_METHOD("optimize"), &SceneOptimizePlugin::optimize);
}

void SceneOptimizePlugin::_notification(int notification) {
	if (notification == NOTIFICATION_ENTER_TREE) {
		editor->add_tool_menu_item("Optimize Scene", this, "optimize");
	} else if (notification == NOTIFICATION_EXIT_TREE) {
		editor->remove_tool_menu_item("Optimize Scene");
	}
}

SceneOptimizePlugin::SceneOptimizePlugin(EditorNode *p_node) {
	editor = p_node;
}
