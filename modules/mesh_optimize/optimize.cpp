/*************************************************************************/
/*  optimize.cpp                                                         */
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
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN connect_compatION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

// Based on
// https://github.com/zeux/meshoptimizer/blob/bce99a4bfdc7bbc72479e1d71c4083329d306347/demo/main.cpp

#include "optimize.h"

#include "core/object.h"
#include "core/project_settings.h"
#include "core/vector.h"
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

void MeshOptimize::_node_replace_owner(Node *p_base, Node *p_node, Node *p_root) {

	p_node->set_owner(p_root);
	p_node->set_filename("");

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_node_replace_owner(p_base, p_node->get_child(i), p_root);
	}
}

void MeshOptimize::optimize(const String p_file, Node *p_root_node) {
	PackedScene *scene = memnew(PackedScene);
	scene->pack(p_root_node);
	Node *root = scene->instance();
	_node_replace_owner(root, root, root);
	simplify(root);
	scene->pack(root);
	ResourceSaver::save(p_file, scene);
}

void MeshOptimize::simplify(Node *p_root_node) {
	Vector<MeshInstance *> mesh_items;
	_find_all_mesh_instances(mesh_items, p_root_node, p_root_node);

	if (!mesh_items.size()) {
		return;
	}

	Vector<MeshInfo> meshes;
	for (int32_t i = 0; i < mesh_items.size(); i++) {
		MeshInfo mesh_info;
		mesh_info.mesh = mesh_items[i]->get_mesh();
		mesh_info.transform = mesh_items[i]->get_transform();
		mesh_info.name = mesh_items[i]->get_name();
		mesh_info.original_node = mesh_items[i];
		mesh_info.skin = mesh_items[i]->get_skin();
		mesh_info.skeleton_path = mesh_items[i]->get_skeleton_path();
		meshes.push_back(mesh_info);
	}

	const size_t lod_count = 4;
	struct Vertex {
		float px, py, pz;
	};

	for (int32_t i = 0; i < meshes.size(); i++) {
		Vector<Ref<Mesh> > lod_meshes;
		Ref<Mesh> mesh = meshes[i].mesh;
		if (mesh->get_blend_shape_count()) {
			// Don't lod blend shapes.
			MeshInstance *mi = memnew(MeshInstance);
			mi->set_mesh(mesh);
			mi->set_skeleton_path(meshes[i].skeleton_path);
			mi->set_name(meshes[i].name);
			mi->set_skin(meshes[i].skin);
			meshes[i].original_node->replace_by(mi, true);
			mi->set_owner(p_root_node);
			continue;
		}
		for (int32_t count_i = 0; count_i < lod_count; count_i++) {
			Ref<ArrayMesh> result_mesh;
			result_mesh.instance();
			for (int32_t j = 0; j < mesh->get_surface_count(); j++) {
				Ref<SurfaceTool> st;
				st.instance();
				st->begin(Mesh::PRIMITIVE_TRIANGLES);
				st->create_from(mesh, j);
				st->index();
				const Array mesh_array = st->commit_to_arrays();
				Vector<Vector3> vertexes = mesh_array[Mesh::ARRAY_VERTEX];
				// https://github.com/zeux/meshoptimizer/blob/bce99a4bfdc7bbc72479e1d71c4083329d306347/demo/main.cpp#L414
				// generate LOD levels, with each subsequent LOD using 70% triangles
				// note that each LOD uses the same (shared) vertex buffer
				Vector<Vector<uint32_t> > lods;
				lods.resize(2);
				Vector<uint32_t> unsigned_indices;
				{
					Vector<int32_t> indices = mesh_array[Mesh::ARRAY_INDEX];
					unsigned_indices.resize(indices.size());
					for (int32_t o = 0; o < indices.size(); o++) {
						unsigned_indices.write[o] = indices[o];
					}
				}
				lods.write[0] = unsigned_indices;
				Vector<Vertex> meshopt_vertices;
				meshopt_vertices.resize(vertexes.size());

				for (int32_t k = 0; k < vertexes.size(); k++) {
					Vertex meshopt_vertex;
					Vector3 vertex = vertexes[k];
					meshopt_vertex.px = vertex.x;
					meshopt_vertex.py = vertex.y;
					meshopt_vertex.pz = vertex.z;
					meshopt_vertices.write[k] = meshopt_vertex;
				}
				
				// simplifying from the base level sometimes produces better results

				const int32_t current_lod = lods.size() - 1;

				Vector<uint32_t> &lod = lods.write[current_lod];

				float threshold = powf(0.7f, float(count_i));
				size_t target_index_count = (unsigned_indices.size() * threshold) / 3 * 3;
				float target_error = 1e-2f;

				if (unsigned_indices.size() < target_index_count) {
					target_index_count = unsigned_indices.size();
				}

				lod.resize(unsigned_indices.size());
				lod.resize(meshopt_simplify(&lod.write[0], unsigned_indices.ptr(), unsigned_indices.size(), &meshopt_vertices[0].px, meshopt_vertices.size(), sizeof(Vertex), target_index_count, target_error));

				meshopt_optimizeVertexCache(&lod.write[0], &lod.write[0], lod.size(), meshopt_vertices.size());
				meshopt_optimizeOverdraw(&lod.write[0], &lod[0], lod.size(), &meshopt_vertices[0].px, meshopt_vertices.size(), sizeof(Vertex), 1.0f);

				// TODO
				// concatenate all LODs into one IB
				// note: the order of concatenation is important - since we optimize the entire IB for vertex fetch,
				// putting coarse LODs first makes sure that the vertex range referenced by them is as small as possible
				// some GPUs process the entire range referenced by the index buffer region so doing this optimizes the vertex transform
				// cost for coarse LODs
				// this order also produces much better vertex fetch cache coherency for coarse LODs (since they're essentially optimized first)
				// somewhat surprisingly, the vertex fetch cache coherency for fine LODs doesn't seem to suffer that much.

				Array current_mesh = mesh_array;
				Vector<int32_t> indexes = current_mesh[Mesh::ARRAY_INDEX];
				indexes.resize(lods[current_lod].size());
				for (int32_t p = 0; p < lods[current_lod].size(); p++) {
					indexes.write[p] = lods[current_lod][p];
				}
				current_mesh[Mesh::ARRAY_INDEX] = indexes;
				result_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, current_mesh);

				if (mesh->surface_get_material(j).is_valid()) {
					result_mesh->surface_set_material(j, mesh->surface_get_material(j));
				}
			}
			MeshInstance *mi = memnew(MeshInstance);
			mi->set_mesh(result_mesh);
			mi->set_skeleton_path(meshes[i].skeleton_path);
			mi->set_name(String(meshes[i].name) + "Lod" + itos(count_i));
			mi->set_skin(meshes[i].skin);
			if (meshes[i].original_node) {
				Spatial *spatial = Object::cast_to<Spatial>(meshes[i].original_node);
				if (spatial) {
					mi->set_transform(spatial->get_transform());
				}
				meshes[i].original_node->get_parent()->add_child(mi);
				mi->set_owner(meshes[i].original_node->get_owner());
			}
		}
	}
}

void MeshOptimize::_find_all_mesh_instances(Vector<MeshInstance *> &r_items, Node *p_current_node, const Node *p_owner) {
	MeshInstance *mi = Object::cast_to<MeshInstance>(p_current_node);
	if (mi != NULL && mi->get_mesh().is_valid()) {
		r_items.push_back(mi);
	}
	for (int32_t i = 0; i < p_current_node->get_child_count(); i++) {
		_find_all_mesh_instances(r_items, p_current_node->get_child(i), p_owner);
	}
}

#endif

void MeshOptimizePlugin::optimize(Variant p_user_data) {
	file_export_lib = memnew(EditorFileDialog);
	file_export_lib->set_title(TTR("Export Library"));
	file_export_lib->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	file_export_lib->connect_compat("file_selected", this, "_dialog_action");
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

void MeshOptimizePlugin::_dialog_action(String p_file) {
	Node *node = editor->get_tree()->get_edited_scene_root();
	if (!node) {
		editor->show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
		return;
	}
	if (FileAccess::exists(p_file) && file_export_lib_merge->is_pressed()) {
		Ref<PackedScene> scene = ResourceLoader::load(p_file, "PackedScene");
		if (scene.is_null()) {
			editor->show_accept(TTR("Can't load scene for mesh optimize!"), TTR("OK"));
			return;
		} else {
			node->add_child(scene->instance());
		}
	}
	scene_optimize->optimize(p_file, node);
	EditorFileSystem::get_singleton()->scan_changes();
	file_export_lib->queue_delete();
	file_export_lib_merge->queue_delete();
}
void MeshOptimizePlugin::_bind_methods() {
	ClassDB::bind_method("_dialog_action", &MeshOptimizePlugin::_dialog_action);
	ClassDB::bind_method(D_METHOD("optimize"), &MeshOptimizePlugin::optimize);
}

void MeshOptimizePlugin::_notification(int notification) {
	if (notification == NOTIFICATION_ENTER_TREE) {
		editor->add_tool_menu_item("Optimize Meshes", this, "optimize");
	} else if (notification == NOTIFICATION_EXIT_TREE) {
		editor->remove_tool_menu_item("Optimize Meshes");
	}
}

MeshOptimizePlugin::MeshOptimizePlugin(EditorNode *p_node) {
	editor = p_node;
}
