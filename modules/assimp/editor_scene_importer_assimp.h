/*************************************************************************/
/*  editor_scene_importer_assimp.h                                       */
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

#ifndef EDITOR_SCENE_IMPORTER_ASSIMP_H
#define EDITOR_SCENE_IMPORTER_ASSIMP_H

#ifdef TOOLS_ENABLED
#include "core/bind/core_bind.h"
#include "core/io/resource_importer.h"
#include "core/vector.h"
#include "editor/import/resource_importer_scene.h"
#include "editor/project_settings_editor.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/skeleton.h"
#include "scene/3d/spatial.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/animation.h"
#include "scene/resources/surface_tool.h"

#include "assimp/DefaultLogger.hpp"
#include "assimp/LogStream.hpp"
#include "assimp/Logger.hpp"
#include "assimp/matrix4x4.h"
#include "assimp/scene.h"
#include "assimp/types.h"

class AssimpStream : public Assimp::LogStream {
public:
	// Constructor
	AssimpStream() {}

	// Destructor
	~AssimpStream() {}
	// Write something using your own functionality
	void write(const char *message) {
		print_verbose(String("Open Asset Import: ") + String(message).strip_edges());
	}
};

#define AI_MATKEY_FBX_MAYA_BASE_COLOR_FACTOR "$raw.Maya|baseColor", 0, 0
#define AI_MATKEY_FBX_MAYA_METALNESS_FACTOR "$raw.Maya|metalness", 0, 0
#define AI_MATKEY_FBX_MAYA_DIFFUSE_ROUGHNESS_FACTOR "$raw.Maya|diffuseRoughness", 0, 0

#define AI_MATKEY_FBX_MAYA_METALNESS_TEXTURE "$raw.Maya|metalness|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_METALNESS_UV_XFORM "$raw.Maya|metalness|uvtrafo", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_DIFFUSE_ROUGHNESS_TEXTURE "$raw.Maya|diffuseRoughness|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_DIFFUSE_ROUGHNESS_UV_XFORM "$raw.Maya|diffuseRoughness|uvtrafo", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_BASE_COLOR_TEXTURE "$raw.Maya|baseColor|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_BASE_COLOR_UV_XFORM "$raw.Maya|baseColor|uvtrafo", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_NORMAL_TEXTURE "$raw.Maya|normalCamera|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_NORMAL_UV_XFORM "$raw.Maya|normalCamera|uvtrafo", aiTextureType_UNKNOWN, 0

#define AI_MATKEY_FBX_NORMAL_TEXTURE "$raw.Maya|normalCamera|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_NORMAL_UV_XFORM "$raw.Maya|normalCamera|uvtrafo", aiTextureType_UNKNOWN, 0

#define AI_MATKEY_FBX_MAYA_STINGRAY_DISPLACEMENT_SCALING_FACTOR "$raw.Maya|displacementscaling", 0, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_BASE_COLOR_FACTOR "$raw.Maya|base_color", 0, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_FACTOR "$raw.Maya|emissive", 0, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_METALLIC_FACTOR "$raw.Maya|metallic", 0, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_ROUGHNESS_FACTOR "$raw.Maya|roughness", 0, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_INTENSITY_FACTOR "$raw.Maya|emissive_intensity", 0, 0

#define AI_MATKEY_FBX_MAYA_STINGRAY_NORMAL_TEXTURE "$raw.Maya|TEX_normal_map|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_NORMAL_UV_XFORM "$raw.Maya|TEX_normal_map|uvtrafo", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_COLOR_TEXTURE "$raw.Maya|TEX_color_map|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_COLOR_UV_XFORM "$raw.Maya|TEX_color_map|uvtrafo", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_METALLIC_TEXTURE "$raw.Maya|TEX_metallic_map|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_METALLIC_UV_XFORM "$raw.Maya|TEX_metallic_map|uvtrafo", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_ROUGHNESS_TEXTURE "$raw.Maya|TEX_roughness_map|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_ROUGHNESS_UV_XFORM "$raw.Maya|TEX_roughness_map|uvtrafo", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_TEXTURE "$raw.Maya|TEX_emissive_map|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_UV_XFORM "$raw.Maya|TEX_emissive_map|uvtrafo", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_AO_TEXTURE "$raw.Maya|TEX_ao_map|file", aiTextureType_UNKNOWN, 0
#define AI_MATKEY_FBX_MAYA_STINGRAY_AO_UV_XFORM "$raw.Maya|TEX_ao_map|uvtrafo", aiTextureType_UNKNOWN, 0

class EditorSceneImporterAssimp : public EditorSceneImporter {
private:
	GDCLASS(EditorSceneImporterAssimp, EditorSceneImporter);
	const String ASSIMP_FBX_KEY = "_$AssimpFbx$";

	struct AssetImportAnimation {
		enum Interpolation {
			INTERP_LINEAR,
			INTERP_STEP,
			INTERP_CATMULLROMSPLINE,
			INTERP_CUBIC_SPLINE
		};
	};

	struct ImportState {

		String path;
		const aiScene *assimp_scene;
		uint32_t max_bone_weights;

		Spatial *root;
		Map<String, Ref<Mesh> > mesh_cache;
		Map<int, Ref<Material> > material_cache;
		Map<String, int> light_cache;
		Map<String, int> camera_cache;
		//Vector<Skeleton *> skeletons;
		Map<Skeleton *, const Node *> armature_skeletons; // maps skeletons based on their armature nodes.
		Map<const aiBone *, Skeleton *> bone_to_skeleton_lookup; // maps bones back into their skeleton
		// very useful for when you need to ask assimp for the bone mesh
		Map<String, Node *> node_map;
		Map<const aiNode *, const Node *> assimp_node_map;
		bool fbx; //for some reason assimp does some things different for FBX
		AnimationPlayer *animation_player;
	};

	/** Recursive state is used to push state into functions instead of specifying them
	* This makes the code easier to handle too and add extra arguments without breaking things
	*/
	struct RecursiveState {
		RecursiveState(
				Transform &_node_transform,
				Skeleton *_skeleton,
				Spatial *_new_node,
				const String &_node_name,
				const aiNode *_assimp_node,
				Node *_parent_node,
				const aiBone *_bone) :
				node_transform(_node_transform),
				skeleton(_skeleton),
				new_node(_new_node),
				node_name(_node_name),
				assimp_node(_assimp_node),
				parent_node(_parent_node),
				bone(_bone) {}

		Transform &node_transform;
		Skeleton *skeleton;
		Spatial *new_node;
		const String &node_name;
		const aiNode *assimp_node;
		Node *parent_node;
		const aiBone *bone;
	};

	struct BoneInfo {
		uint32_t bone;
		float weight;
	};

	struct SkeletonHole { //nodes may be part of the skeleton by used by vertex
		String name;
		String parent;
		Transform pose;
		const aiNode *node;
	};

	void _calc_tangent_from_mesh(const aiMesh *ai_mesh, int i, int tri_index, int index, PoolColorArray::Write &w);
	void _set_texture_mapping_mode(aiTextureMapMode *map_mode, Ref<Texture> texture);

	Ref<Texture> _load_texture(ImportState &state, String p_path);
	Ref<Material> _generate_material_from_index(ImportState &state, int p_index, bool p_double_sided);
	Ref<Mesh> _generate_mesh_from_surface_indices(ImportState &state, Transform *parent_node, const Vector<int> &p_surface_indices, Skeleton *p_skeleton = NULL, bool p_double_sided_material = false);

	// utility for node creation
	void attach_new_node(ImportState &state, Spatial *new_node, const aiNode *node, Node *parent_node, String Name, Transform &transform);
	// simple object creation functions
	void create_light(ImportState &state, RecursiveState &recursive_state);
	void create_camera(ImportState &state, RecursiveState &recursive_state);
	void create_bone(ImportState &state, RecursiveState &recursive_state);
	void create_mesh(ImportState &state, RecursiveState &recursive_state);

	// recursive node generator
	void _generate_node(ImportState &state, Skeleton *skeleton, const aiNode *assimp_node, Node *parent_node);
	// runs after _generate_node as it must then use pre-created godot skeleton.
	void generate_mesh_phase_from_skeletal_mesh(ImportState &state, const aiNode *assimp_node, Node *parent_node);
	void _insert_animation_track(ImportState &scene, const aiAnimation *assimp_anim, int p_track, int p_bake_fps, Ref<Animation> animation, float ticks_per_second, Skeleton *p_skeleton, const NodePath &p_path, const String &p_name);

	void _import_animation(ImportState &state, int p_animation_index, int p_bake_fps);

	Spatial *_generate_scene(const String &p_path, aiScene *scene, const uint32_t p_flags, int p_bake_fps, const int32_t p_max_bone_weights);

	String _assimp_anim_string_to_string(const aiString &p_string) const;
	String _assimp_raw_string_to_string(const aiString &p_string) const;
	float _get_fbx_fps(int32_t time_mode, const aiScene *p_scene);
	template <class T>
	T _interpolate_track(const Vector<float> &p_times, const Vector<T> &p_values, float p_time, AssetImportAnimation::Interpolation p_interp);
	void _register_project_setting_import(const String generic, const String import_setting_string, const Vector<String> &exts, List<String> *r_extensions, const bool p_enabled) const;

	struct ImportFormat {
		Vector<String> extensions;
		bool is_default;
	};

protected:
	static void _bind_methods();

public:
	EditorSceneImporterAssimp() {
		Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
		unsigned int severity = Assimp::Logger::Info | Assimp::Logger::Err | Assimp::Logger::Warn;
		Assimp::DefaultLogger::get()->attachStream(new AssimpStream(), severity);
	}
	~EditorSceneImporterAssimp() {
		Assimp::DefaultLogger::kill();
	}

	virtual void get_extensions(List<String> *r_extensions) const;
	virtual uint32_t get_import_flags() const;
	virtual Node *import_scene(const String &p_path, uint32_t p_flags, int p_bake_fps, List<String> *r_missing_deps, Error *r_err = NULL);
};
#endif
#endif
