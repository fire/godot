#include "editor_scene_exporter_gltf.h"
#include "editor_scene_exporter_gltf_plugin.h"
#include "core/object.h"
#include "core/project_settings.h"
#include "core/vector.h"
#include "editor/saver/resource_saver_scene.h"
#include "scene/3d/mesh_instance.h"
#include "scene/gui/check_box.h"
#include "scene/main/node.h"
#include "editor_scene_exporter_gltf.h"
#include "editor/editor_file_system.h"

#include "editor/editor_node.h"

String SceneExporterGLTFPlugin::get_name() const {
	return "ConvertGLTF2";
}

void SceneExporterGLTFPlugin::_bind_methods() {
	ClassDB::bind_method("_gltf2_dialog_action", &SceneExporterGLTFPlugin::_gltf2_dialog_action);
	ClassDB::bind_method(D_METHOD("convert_scene_to_gltf2"), &SceneExporterGLTFPlugin::convert_scene_to_gltf2);
}

void SceneExporterGLTFPlugin::_notification(int notification) {
	if (notification == NOTIFICATION_ENTER_TREE) {
		editor->add_tool_menu_item("Convert Scene to GLTF", this, "convert_scene_to_gltf2");
	} else if (notification == NOTIFICATION_EXIT_TREE) {
		editor->remove_tool_menu_item("Convert Scene to GLTF");
	}
}

bool SceneExporterGLTFPlugin::has_main_screen() const {
	return false;
}

SceneExporterGLTFPlugin::SceneExporterGLTFPlugin(EditorNode *p_node) {
	editor = p_node;
}

void SceneExporterGLTFPlugin::_gltf2_dialog_action(String p_file) {
	Node *root = editor->get_tree()->get_edited_scene_root();
	if (!root) {
		editor->show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
		return;
	}
	if (FileAccess::exists(p_file) && file_export_lib_merge->is_pressed()) {
		Ref<PackedScene> scene = ResourceLoader::load(p_file, "PackedScene");
		if (scene.is_null()) {
			editor->show_accept(TTR("Can't load scene for merging!"), TTR("OK"));
			return;
		} else {
			root->add_child(scene->instance());
		}
	}
	convert_gltf2->export_gltf2(p_file, root);
	EditorFileSystem::get_singleton()->scan_changes();
	file_export_lib->queue_delete();
	file_export_lib_merge->queue_delete();
}

void SceneExporterGLTFPlugin::convert_scene_to_gltf2(Variant p_user_data) {
	file_export_lib = memnew(EditorFileDialog);
	file_export_lib->set_title(TTR("Export Library"));
	file_export_lib->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	file_export_lib_merge = memnew(CheckBox);
	file_export_lib_merge->set_text(TTR("Merge With Existing"));
	file_export_lib_merge->set_pressed(false);
	file_export_lib->get_vbox()->add_child(file_export_lib_merge);
	editor->get_gui_base()->add_child(file_export_lib);
	file_export_lib->clear_filters();
	file_export_lib->add_filter("*.glb");
	file_export_lib->popup_centered_ratio();
	file_export_lib->set_title(TTR("Export Mesh GLTF2"));
	file_export_lib->connect("file_selected", this, "_gltf2_dialog_action");
}