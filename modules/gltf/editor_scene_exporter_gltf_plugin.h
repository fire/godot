#ifndef EDITOR_SCENE_EXPORTER_GLTF_PLUGIN_H
#define EDITOR_SCENE_EXPORTER_GLTF_PLUGIN_H
#include "editor_scene_exporter_gltf.h"

class SceneExporterGLTFPlugin : public EditorPlugin {

	GDCLASS(SceneExporterGLTFPlugin, EditorPlugin);

	Ref<EditorSceneExporterGLTF> convert_gltf2;
	EditorNode *editor;
	CheckBox *file_export_lib_merge;
	EditorFileDialog *file_export_lib;

protected:
	static void _bind_methods();

public:
	void _gltf_dialog_action(String p_file);
	void convert_scene_to_gltf(Variant p_user_data);
	virtual String get_name() const;
	virtual void _notification(int notification);
	bool has_main_screen() const;

	SceneExporterGLTFPlugin(class EditorNode *p_node);
	void _gltf2_dialog_action(String p_file);
	void convert_scene_to_gltf2(Variant p_user_data);
};

#endif
