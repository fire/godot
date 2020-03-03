/*************************************************************************/
/*  optimize.h                                                           */
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

#ifndef SCENE_OPTIMIZE_H
#define SCENE_OPTIMIZE_H

#ifdef TOOLS_ENABLED
#include "core/bind/core_bind.h"
#include "core/reference.h"
#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "modules/csg/csg_shape.h"
#include "modules/gridmap/grid_map.h"
#include "scene/3d/mesh_instance.h"
#include "scene/main/node.h"

class MeshOptimize : public Reference {
private:
	GDCLASS(MeshOptimize, Reference);

	void _find_all_mesh_instances(Vector<MeshInstance *> &r_items, Node *p_current_node, const Node *p_owner);
	void _dialog_action(String p_file);
	void _node_replace_owner(Node *p_base, Node *p_node, Node *p_root);

public:
	struct MeshInfo {
		Transform transform;
		Ref<Mesh> mesh;
		String name;
		Node *original_node;
		NodePath skeleton_path;
		Ref<Skin> skin;
	};
	void optimize(const String p_file, Node *p_root_node);
	void simplify(Node *p_root_node);
};

class MeshOptimizePlugin : public EditorPlugin {

	GDCLASS(MeshOptimizePlugin, EditorPlugin);

	EditorNode *editor;
	CheckBox *file_export_lib_merge;
	EditorFileDialog *file_export_lib;
	Ref<MeshOptimize> scene_optimize;
	void _dialog_action(String p_file);

protected:
	static void _bind_methods();

public:
	MeshOptimizePlugin(EditorNode *p_node);
	void _notification(int notification);
	void optimize(Variant p_user_data);
};

#endif
#endif
