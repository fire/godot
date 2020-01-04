#include "editor_scene_exporter_gltf.h"
#include "editor_scene_importer_gltf.h"

class SceneImportExport : public Node {

	GDCLASS(SceneImportExport, Node);
	Ref<EditorSceneExporterGLTF> export_gltf2;
	Ref<EditorSceneImporterGLTF> import_gltf2;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("export_gltf", "node", "path", "flags", "bake_fps"), &SceneImportExport::export_gltf, DEFVAL(0), DEFVAL(1000.0f));
		ClassDB::bind_method(D_METHOD("import_gltf", "path", "flags", "bake_fps"), &SceneImportExport::import_gltf, DEFVAL(0), DEFVAL(1000.0f));
	}

public:
	Error export_gltf(Node *p_root, String p_path, int32_t p_flags = 0, real_t p_bake_fps = 1000.0f) {

		Error err;
		List<String> deps;
		export_gltf2->save_scene(p_root, p_path, "", p_flags, p_bake_fps, &deps, &err);
		return err;
	}

	Node *import_gltf(String p_path, int32_t p_flags = 0, real_t p_bake_fps = 1000.0f) {
		Error err;
		List<String> deps;
		Node *root = import_gltf2->import_scene(p_path, p_flags, p_bake_fps, &deps, &err);
		ERR_FAIL_COND_V(err != OK, NULL);
		return root;
	}
	SceneImportExport() {
		import_gltf2.instance();
		export_gltf2.instance();
	}
	~SceneImportExport() {
	}
};