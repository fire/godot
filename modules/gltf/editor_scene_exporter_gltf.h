#include "core/object.h"
#include "core/project_settings.h"
#include "core/vector.h"
#include "editor/editor_plugin.h"
#include "editor/saver/editor_saver_plugin.h"
#include "editor/saver/resource_saver_scene.h"
#include "modules/csg/csg_shape.h"
#include "modules/gridmap/grid_map.h"
#include "scene/3d/mesh_instance.h"
#include "scene/gui/check_box.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/surface_tool.h"

#ifndef EDITOR_SCENE_EXPORTER_GLTF_H
#define EDITOR_SCENE_EXPORTER_GLTF_H
class EditorSceneExporter;
class EditorSceneExporterGLTF : public EditorSceneExporter {

	GDCLASS(EditorSceneExporterGLTF, EditorSceneExporter);
public:
	virtual void save_scene(const Node *p_node, const String &p_path, uint32_t p_flags, int p_bake_fps, Error *err = NULL) {}

	struct MeshInfo {
		Transform transform;
		Ref<Mesh> mesh;
		String name;
		Vector<Ref<Material> > materials;
		Node *original_node = NULL;
	};
	void _find_all_mesh_instances(Vector<MeshInstance *> &r_items, Node *p_current_node, const Node *p_owner);
	void _find_all_gridmaps(Vector<GridMap *> &r_items, Node *p_current_node, const Node *p_owner);
	void _find_all_csg_roots(Vector<CSGShape *> &r_items, Node *p_current_node, const Node *p_owner);
	// void _set_gltf_materials(Ref<SpatialMaterial> &mat, aiMaterial *assimp_mat);
	Error _generate_gltf_scene(const String p_path, Spatial *p_root_node);
	// void _generate_node(Node *p_node, size_t &num_meshes, /*aiNode *&p_assimp_current_node, aiNode *&p_assimp_root, */ Vector<aiMesh *> &assimp_meshes, Vector<aiMaterial *> &assimp_materials);

	void export_gltf2(const String p_file, Node *p_root_node);

	EditorSceneExporterGLTF() {}
	~EditorSceneExporterGLTF() {}
};

#endif
