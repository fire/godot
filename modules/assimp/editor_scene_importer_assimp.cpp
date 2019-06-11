/*************************************************************************/
/*  editor_scene_importer_assimp.cpp                                     */
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

#include "assimp/DefaultLogger.hpp"
#include "assimp/Importer.hpp"
#include "assimp/LogStream.hpp"
#include "assimp/Logger.hpp"
#include "assimp/SceneCombiner.h"
#include "assimp/cexport.h"
#include "assimp/cimport.h"
#include "assimp/matrix4x4.h"
#include "assimp/pbrmaterial.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "core/bind/core_bind.h"
#include "core/io/image_loader.h"
#include "editor/editor_file_system.h"
#include "editor/import/resource_importer_scene.h"
#include "editor_scene_importer_assimp.h"
#include "editor_settings.h"
#include "scene/3d/camera.h"
#include "scene/3d/light.h"
#include "scene/3d/mesh_instance.h"
#include "scene/animation/animation_player.h"
#include "scene/main/node.h"
#include "scene/resources/material.h"
#include "scene/resources/surface_tool.h"
#include "zutil.h"
#include <string>

void EditorSceneImporterAssimp::get_extensions(List<String> *r_extensions) const {

	const String import_setting_string = "filesystem/import/open_asset_import/";

	Map<String, ImportFormat> import_format;
	{
		Vector<String> exts;
		exts.push_back("fbx");
		ImportFormat import = { exts, true };
		import_format.insert("fbx", import);
	}
	{
		Vector<String> exts;
		exts.push_back("pmx");
		ImportFormat import = { exts, true };
		import_format.insert("mmd", import);
	}
	for (Map<String, ImportFormat>::Element *E = import_format.front(); E; E = E->next()) {
		_register_project_setting_import(E->key(), import_setting_string, E->get().extensions, r_extensions, E->get().is_default);
	}
}

void EditorSceneImporterAssimp::_register_project_setting_import(const String generic, const String import_setting_string, const Vector<String> &exts, List<String> *r_extensions, const bool p_enabled) const {
	const String use_generic = "use_" + generic;
	_GLOBAL_DEF(import_setting_string + use_generic, p_enabled, true);
	if (ProjectSettings::get_singleton()->get(import_setting_string + use_generic)) {
		for (int32_t i = 0; i < exts.size(); i++) {
			r_extensions->push_back(exts[i]);
		}
	}
}

uint32_t EditorSceneImporterAssimp::get_import_flags() const {
	return EditorSceneImporter::IMPORT_SCENE;
}

AssimpStream::AssimpStream() {
	// empty
}

AssimpStream::~AssimpStream() {
	// empty
}

void AssimpStream::write(const char *message) {
	print_verbose(String("Open Asset Import: ") + String(message).strip_edges());
}

void EditorSceneImporterAssimp::_bind_methods() {
}

Node *EditorSceneImporterAssimp::import_scene(const String &p_path, uint32_t p_flags, int p_bake_fps, List<String> *r_missing_deps, Error *r_err) {
	Assimp::Importer importer;
	std::wstring w_path = ProjectSettings::get_singleton()->globalize_path(p_path).c_str();
	std::string s_path(w_path.begin(), w_path.end());
	importer.SetPropertyBool(AI_CONFIG_PP_FD_REMOVE, true);
	// Cannot remove pivot points because the static mesh will be in the wrong place
	importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, true);
	importer.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, false);
	importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);
	int32_t max_bone_weights = 4;
	//importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, max_bone_weights);
	//if (p_flags & EditorSceneImporter::IMPORT_ANIMATION_8_WEIGHTS) {
	//	const int eight_bones = 8;
	//	importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, eight_bones);
	//	max_bone_weights = eight_bones;
	//}

	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
	//importer.SetPropertyFloat(AI_CONFIG_PP_DB_THRESHOLD, 1.0f);
	int32_t post_process_Steps = //aiProcess_CalcTangentSpace |
			//aiProcess_FlipUVs |
			//aiProcess_FlipWindingOrder |
			//aiProcess_DropNormals |
			//aiProcess_GenSmoothNormals |
			aiProcess_JoinIdenticalVertices |
			aiProcess_ImproveCacheLocality |
			aiProcess_LimitBoneWeights |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_SplitLargeMeshes |
			aiProcess_Triangulate |
			//aiProcess_GenUVCoords |
			//aiProcess_FindDegenerates |
			aiProcess_SortByPType |
			aiProcess_FindInvalidData |
			aiProcess_TransformUVCoords |
			aiProcess_FindInstances |
			//aiProcess_FixInfacingNormals |
			//aiProcess_ValidateDataStructure |
			aiProcess_OptimizeMeshes |
			//aiProcess_OptimizeGraph |
			//aiProcess_Debone |
			//aiProcess_EmbedTextures |
			aiProcess_SplitByBoneCount |
			0;
	const aiScene *ai_scene = importer.ReadFile(s_path.c_str(),
			post_process_Steps);
	ERR_EXPLAIN(String("Open Asset Import failed to open: ") + String(importer.GetErrorString()));
	ERR_FAIL_COND_V(ai_scene == NULL, NULL);
	State state;
	state.scene = ai_scene;
	state.path = p_path;
	state.max_bone_weights = max_bone_weights;
	state.flags = p_flags;
	state.bake_fps = p_bake_fps;
	state.skeleton = memnew(Skeleton);
	state.root = memnew(Spatial);
	Node *scene = _generate_scene(state);
	return scene;
}

template <class T>
struct EditorSceneImporterAssetImportInterpolate {

	T lerp(const T &a, const T &b, float c) const {

		return a + (b - a) * c;
	}

	T catmull_rom(const T &p0, const T &p1, const T &p2, const T &p3, float t) {

		float t2 = t * t;
		float t3 = t2 * t;

		return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4 * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	}

	T bezier(T start, T control_1, T control_2, T end, float t) {
		/* Formula from Wikipedia article on Bezier curves. */
		real_t omt = (1.0 - t);
		real_t omt2 = omt * omt;
		real_t omt3 = omt2 * omt;
		real_t t2 = t * t;
		real_t t3 = t2 * t;

		return start * omt3 + control_1 * omt2 * t * 3.0 + control_2 * omt * t2 * 3.0 + end * t3;
	}
};

//thank you for existing, partial specialization
template <>
struct EditorSceneImporterAssetImportInterpolate<Quat> {

	Quat lerp(const Quat &a, const Quat &b, float c) const {
		ERR_FAIL_COND_V(!a.is_normalized(), Quat());
		ERR_FAIL_COND_V(!b.is_normalized(), Quat());

		return a.slerp(b, c).normalized();
	}

	Quat catmull_rom(const Quat &p0, const Quat &p1, const Quat &p2, const Quat &p3, float c) {
		ERR_FAIL_COND_V(!p1.is_normalized(), Quat());
		ERR_FAIL_COND_V(!p2.is_normalized(), Quat());

		return p1.slerp(p2, c).normalized();
	}

	Quat bezier(Quat start, Quat control_1, Quat control_2, Quat end, float t) {
		ERR_FAIL_COND_V(!start.is_normalized(), Quat());
		ERR_FAIL_COND_V(!end.is_normalized(), Quat());

		return start.slerp(end, t).normalized();
	}
};

template <class T>
T EditorSceneImporterAssimp::_interpolate_track(const Vector<float> &p_times, const Vector<T> &p_values, float p_time, AssetImportAnimation::Interpolation p_interp) {
	//could use binary search, worth it?
	int idx = -1;
	for (int i = 0; i < p_times.size(); i++) {
		if (p_times[i] > p_time)
			break;
		idx++;
	}

	EditorSceneImporterAssetImportInterpolate<T> interp;

	switch (p_interp) {
		case AssetImportAnimation::INTERP_LINEAR: {

			if (idx == -1) {
				return p_values[0];
			} else if (idx >= p_times.size() - 1) {
				return p_values[p_times.size() - 1];
			}

			float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

			return interp.lerp(p_values[idx], p_values[idx + 1], c);

		} break;
		case AssetImportAnimation::INTERP_STEP: {

			if (idx == -1) {
				return p_values[0];
			} else if (idx >= p_times.size() - 1) {
				return p_values[p_times.size() - 1];
			}

			return p_values[idx];

		} break;
		case AssetImportAnimation::INTERP_CATMULLROMSPLINE: {

			if (idx == -1) {
				return p_values[1];
			} else if (idx >= p_times.size() - 1) {
				return p_values[1 + p_times.size() - 1];
			}

			float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

			return interp.catmull_rom(p_values[idx - 1], p_values[idx], p_values[idx + 1], p_values[idx + 3], c);

		} break;
		case AssetImportAnimation::INTERP_CUBIC_SPLINE: {

			if (idx == -1) {
				return p_values[1];
			} else if (idx >= p_times.size() - 1) {
				return p_values[(p_times.size() - 1) * 3 + 1];
			}

			float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

			T from = p_values[idx * 3 + 1];
			T c1 = from + p_values[idx * 3 + 2];
			T to = p_values[idx * 3 + 4];
			T c2 = to + p_values[idx * 3 + 3];

			return interp.bezier(from, c1, c2, to, c);

		} break;
	}

	ERR_FAIL_V(p_values[0]);
}

Spatial *EditorSceneImporterAssimp::_generate_scene(State &state) {
	ERR_FAIL_COND_V(state.scene == NULL, NULL);
	if (state.flags & EditorSceneImporter::IMPORT_ANIMATION) {
		state.ap = memnew(AnimationPlayer);
		state.root->add_child(state.ap);
		state.ap->set_owner(state.root);
		state.ap->set_name(TTR("AnimationPlayer"));
	}
	String ext = state.path.get_file().get_extension().to_lower();
	for (size_t l = 0; l < state.scene->mNumLights; l++) {
		Light *light = NULL;
		aiLight *ai_light = state.scene->mLights[l];
		ERR_CONTINUE(ai_light == NULL);
		if (ai_light->mType == aiLightSource_DIRECTIONAL) {
			light = memnew(DirectionalLight);
			Vector3 dir = Vector3(ai_light->mDirection.y, ai_light->mDirection.x, ai_light->mDirection.z);
			dir.normalize();
			Transform xform;
			Quat quat;
			quat.set_euler(dir);
			Vector3 pos = Vector3(ai_light->mPosition.x, ai_light->mPosition.y, ai_light->mPosition.z);
			xform.origin = pos;
			light->set_transform(xform);
		} else if (ai_light->mType == aiLightSource_POINT) {
			light = memnew(OmniLight);
			Vector3 pos = Vector3(ai_light->mPosition.x, ai_light->mPosition.y, ai_light->mPosition.z);
			Transform xform;
			xform.origin = pos;
			light->set_transform(xform);
			// No idea for energy
			light->set_param(Light::PARAM_ATTENUATION, 0.0f);
		} else if (ai_light->mType == aiLightSource_SPOT) {
			light = memnew(SpotLight);
			Vector3 pos = Vector3(ai_light->mPosition.x, ai_light->mPosition.y, ai_light->mPosition.z);
			Transform xform;
			xform.origin = pos;
			Vector3 dir = Vector3(ai_light->mDirection.y, ai_light->mDirection.x, ai_light->mDirection.z);
			dir.normalize();
			Quat quat;
			quat.set_euler(dir);
			xform.basis = quat;
			light->set_transform(xform);
			// No idea for energy
			light->set_param(Light::PARAM_ATTENUATION, 0.0f);
		}
		ERR_CONTINUE(light == NULL);
		light->set_color(Color(ai_light->mColorDiffuse.r, ai_light->mColorDiffuse.g, ai_light->mColorDiffuse.b));
		state.root->add_child(light);
		light->set_name(_assimp_string_to_string(ai_light->mName));
		light->set_owner(state.root);
		state.light_names.insert(_assimp_string_to_string(state.scene->mLights[l]->mName));
	}
	for (size_t c = 0; c < state.scene->mNumCameras; c++) {
		aiCamera *ai_camera = state.scene->mCameras[c];
		Camera *camera = memnew(Camera);
		float near = ai_camera->mClipPlaneNear;
		if (Math::is_equal_approx(near, 0.0f)) {
			near = 0.1f;
		}
		camera->set_perspective(Math::rad2deg(ai_camera->mHorizontalFOV) * 2.0f, near, ai_camera->mClipPlaneFar);
		Vector3 pos = Vector3(ai_camera->mPosition.x, ai_camera->mPosition.y, ai_camera->mPosition.z);

		Vector3 look_at = Vector3(ai_camera->mLookAt.y, ai_camera->mLookAt.x, ai_camera->mLookAt.z).normalized();
		Quat quat;
		quat.set_euler(look_at);
		Transform xform;
		xform.basis = quat;
		xform.set_origin(pos);
		state.root->add_child(camera);
		camera->set_transform(xform);
		camera->set_name(_assimp_string_to_string(ai_camera->mName));
		camera->set_owner(state.root);
		state.camera_names.insert(_assimp_string_to_string(state.scene->mCameras[c]->mName));
	}
	state.mesh_count = 0;
	for (size_t i = 0; i < state.scene->mRootNode->mNumChildren; i++) {
		_generate_node(state, state.scene->mRootNode->mChildren[i], state.root, state.root);
	}

	aiNode *skeleton_root = NULL;
	for (int32_t i = 0; i < state.skeleton->get_bone_count(); i++) {
		if (state.skeleton->get_bone_parent(i) == -1) {
			skeleton_root = _assimp_find_node(state.scene->mRootNode, state.skeleton->get_bone_name(i));
			break;
		}
	}

	for (int32_t i = 0; i < state.skeleton->get_bone_count(); i++) {
		aiNode *node = skeleton_root;
		while (node != state.scene->mRootNode && node->mParent != state.scene->mRootNode) {
			while (node->mParent) {
				if (_assimp_string_to_string(node->mName).split(ASSIMP_FBX_KEY)[0] != _assimp_string_to_string(node->mParent->mName).split(ASSIMP_FBX_KEY)[0]) {
					break;
				}
				node = node->mParent;
			}
			node = node->mParent;
		}
		const aiNode *armature_node = node;
		if (armature_node == state.scene->mRootNode) {
			state.root->add_child(state.skeleton);
			state.skeleton->set_owner(state.root);
			break;
		}
		state.root->find_node(_assimp_string_to_string(armature_node->mName))->get_parent()->add_child(state.skeleton);
		state.skeleton->set_owner(state.root);
	}
	state.skeleton->localize_rests();
	for (Map<MeshInstance *, Skeleton *>::Element *E = state.mesh_skeletons.front(); E; E = E->next()) {
		String path = String(E->key()->get_path_to(E->key()->get_owner()));
		if (E->key()->get_parent() == state.root) {
			E->key()->set_skeleton_path("../" + state.root->get_path_to(E->get()));
		} else {
			E->key()->set_skeleton_path(path + "/" + E->get()->get_owner()->get_path_to(E->get()));
		}
	}
	if (state.flags & EditorSceneImporter::IMPORT_ANIMATION) {
		for (size_t i = 0; i < state.scene->mNumAnimations; i++) {
			_import_animation(state, i);
		}
		List<StringName> animation_names;
		state.ap->get_animation_list(&animation_names);
		if (animation_names.empty()) {
			state.root->remove_child(state.ap);
			memdelete(state.ap);
		}
	}
	return state.root;
}

String EditorSceneImporterAssimp::_find_skeleton_bone_root(Map<Skeleton *, MeshInstance *> &skeletons, Map<MeshInstance *, String> &meshes, Spatial *root) {
	for (Map<Skeleton *, MeshInstance *>::Element *E = skeletons.front(); E; E = E->next()) {
		if (meshes.has(E->get())) {
			String name = meshes[E->get()];
			if (name != "") {
				return name;
			}
		}
	}
	return "";
}

void EditorSceneImporterAssimp::_insert_animation_track(const aiScene *p_scene, const String p_path, int p_bake_fps, Ref<Animation> animation, float ticks_per_second, float length, const Skeleton *sk, const aiNodeAnim *track, String node_name, NodePath node_path) {

	if (track->mNumRotationKeys || track->mNumPositionKeys || track->mNumScalingKeys) {
		int track_idx = animation->get_track_count();
		animation->add_track(Animation::TYPE_TRANSFORM);
		animation->track_set_path(track_idx, node_path);

		float increment = 1.0 / float(p_bake_fps);
		float time = 0.0;

		Vector3 base_pos;
		Quat base_rot;
		Vector3 base_scale = Vector3(1, 1, 1);

		bool last = false;

		Vector<Vector3> pos_values;
		Vector<float> pos_times;
		Vector<Vector3> scale_values;
		Vector<float> scale_times;
		Vector<Quat> rot_values;
		Vector<float> rot_times;

		for (size_t p = 0; p < track->mNumPositionKeys; p++) {
			aiVector3D pos = track->mPositionKeys[p].mValue;
			pos_values.push_back(Vector3(pos.x, pos.y, pos.z));
			pos_times.push_back(track->mPositionKeys[p].mTime / ticks_per_second);
		}

		for (size_t r = 0; r < track->mNumRotationKeys; r++) {
			aiQuaternion quat = track->mRotationKeys[r].mValue;
			rot_values.push_back(Quat(quat.x, quat.y, quat.z, quat.w).normalized());
			rot_times.push_back(track->mRotationKeys[r].mTime / ticks_per_second);
		}

		for (size_t sc = 0; sc < track->mNumScalingKeys; sc++) {
			aiVector3D scale = track->mScalingKeys[sc].mValue;
			scale_values.push_back(Vector3(scale.x, scale.y, scale.z));
			scale_times.push_back(track->mScalingKeys[sc].mTime / ticks_per_second);
		}
		while (true) {
			Vector3 pos = base_pos;
			Quat rot = base_rot;
			Vector3 scale = base_scale;

			if (pos_values.size()) {
				pos = _interpolate_track<Vector3>(pos_times, pos_values, time, AssetImportAnimation::INTERP_LINEAR);
			}

			if (rot_values.size()) {
				rot = _interpolate_track<Quat>(rot_times, rot_values, time, AssetImportAnimation::INTERP_LINEAR).normalized();
			}

			if (scale_values.size()) {
				scale = _interpolate_track<Vector3>(scale_times, scale_values, time, AssetImportAnimation::INTERP_LINEAR);
			}

			if (sk != NULL && sk->find_bone(node_name) != -1) {
				Transform xform;
				xform.basis.set_quat_scale(rot, scale);
				xform.origin = pos;

				int bone = sk->find_bone(node_name);
				Transform rest_xform = sk->get_bone_rest(bone);
				xform = rest_xform.affine_inverse() * xform;
				rot = xform.basis.get_rotation_quat();
				scale = xform.basis.get_scale();
				pos = xform.origin;
			}
			rot.normalize();

			animation->track_set_interpolation_type(track_idx, Animation::INTERPOLATION_LINEAR);
			animation->transform_track_insert_key(track_idx, time, pos, rot, scale);

			if (last) {
				break;
			}
			time += increment;
			if (time >= length) {
				last = true;
				time = length;
			}
		}
	}
}

void EditorSceneImporterAssimp::_import_animation(State &state, int32_t p_index) {
	String name = "Animation";
	aiAnimation const *anim = NULL;
	if (p_index != -1) {
		anim = state.scene->mAnimations[p_index];
		if (anim->mName.length > 0) {
			name = _assimp_anim_string_to_string(anim->mName);
		}
	}

	Ref<Animation> animation;
	animation.instance();
	float length = 0.0f;
	animation->set_name(name);
	float ticks_per_second = state.scene->mAnimations[p_index]->mTicksPerSecond;

	if (state.scene->mMetaData != NULL && Math::is_equal_approx(ticks_per_second, 0.0f)) {
		int32_t time_mode = 0;
		state.scene->mMetaData->Get("TimeMode", time_mode);
		ticks_per_second = _get_fbx_fps(time_mode, state.scene);
	}

	if ((state.path.get_file().get_extension().to_lower() == "glb" || state.path.get_file().get_extension().to_lower() == "gltf") && Math::is_equal_approx(ticks_per_second, 0.0f)) {
		ticks_per_second = 1000.0f;
	}

	if (Math::is_equal_approx(ticks_per_second, 0.0f)) {
		ticks_per_second = 25.0f;
	}

	length = anim->mDuration / ticks_per_second;
	if (anim) {
		Map<String, Vector<const aiNodeAnim *> > node_tracks;
		for (size_t i = 0; i < anim->mNumChannels; i++) {
			const aiNodeAnim *track = anim->mChannels[i];
			String node_name = _assimp_string_to_string(track->mNodeName);
			NodePath node_path = node_name;
			bool is_bone = false;
			if (node_name.split(ASSIMP_FBX_KEY).size() > 1) {
				String p_track_type = node_name.split(ASSIMP_FBX_KEY)[1];
				if (p_track_type == "_Translation" || p_track_type == "_Rotation" || p_track_type == "_Scaling") {
					continue;
				}
			}
			for (Map<Skeleton *, MeshInstance *>::Element *E = state.skeletons.front(); E; E = E->next()) {
				Skeleton *sk = E->key();
				const String path = state.ap->get_owner()->get_path_to(sk);
				if (path.empty()) {
					continue;
				}
				if (sk->find_bone(node_name) == -1) {
					continue;
				}
				node_path = path + ":" + node_name;
				ERR_CONTINUE(state.ap->get_owner()->has_node(node_path) == false);
				_insert_animation_track(state.scene, state.path, state.bake_fps, animation, ticks_per_second, length, sk, track, node_name, node_path);
				is_bone = true;
			}
			if (is_bone) {
				continue;
			}
			if (!state.ap->get_owner()->find_node(node_name)) {
				continue;
			}
			Node *node = state.ap->get_owner()->find_node(node_name);
			if (!Object::cast_to<MeshInstance>(node) && !Object::cast_to<Light>(node) && !Object::cast_to<Camera>(node)) {
				continue;
			}
			if (state.removed_nodes.has(node_name)) {
				continue;
			}
			const String path = state.ap->get_owner()->get_path_to(node);
			if (path.empty()) {
				print_verbose("Can't animate path");
				continue;
			}
			node_path = path;
			if (state.ap->get_owner()->has_node(node_path) == false) {
				continue;
			}
			_insert_animation_track(state.scene, state.path, state.bake_fps, animation, ticks_per_second, length, NULL, track, node_name, node_path);
		}
		for (size_t i = 0; i < anim->mNumChannels; i++) {
			const aiNodeAnim *track = anim->mChannels[i];
			String node_name = _assimp_string_to_string(track->mNodeName);
			Vector<String> split_name = node_name.split(ASSIMP_FBX_KEY);
			String bare_name = split_name[0];
			Node *node = state.ap->get_owner()->find_node(bare_name);
			if (!Object::cast_to<MeshInstance>(node) && !Object::cast_to<Light>(node) && !Object::cast_to<Camera>(node)) {
				continue;
			}
			if (node != NULL && split_name.size() > 1) {
				Map<String, Vector<const aiNodeAnim *> >::Element *E = node_tracks.find(bare_name);
				Vector<const aiNodeAnim *> ai_tracks;
				if (E) {
					ai_tracks = E->get();
					ai_tracks.push_back(anim->mChannels[i]);
				} else {
					ai_tracks.push_back(anim->mChannels[i]);
				}
				node_tracks.insert(bare_name, ai_tracks);
			}
		}
		for (Map<Skeleton *, MeshInstance *>::Element *E = state.skeletons.front(); E; E = E->next()) {
			Skeleton *sk = E->key();
			Map<String, Vector<const aiNodeAnim *> > anim_tracks;
			for (int32_t i = 0; i < sk->get_bone_count(); i++) {
				String _bone_name = sk->get_bone_name(i);
				Vector<const aiNodeAnim *> ai_tracks;

				if (sk->find_bone(_bone_name) == -1) {
					continue;
				}
				for (size_t j = 0; j < anim->mNumChannels; j++) {
					if (_assimp_string_to_string(anim->mChannels[j]->mNodeName).split(ASSIMP_FBX_KEY).size() == 1) {
						continue;
					}
					String track_name = _assimp_string_to_string(anim->mChannels[j]->mNodeName).split(ASSIMP_FBX_KEY)[0];
					if (track_name != _bone_name) {
						continue;
					}
					if (sk->find_bone(_bone_name) == -1) {
						continue;
					}
					ai_tracks.push_back(anim->mChannels[j]);
				}
				if (ai_tracks.size() == 0) {
					continue;
				}
				anim_tracks.insert(_bone_name, ai_tracks);
			}
			for (Map<String, Vector<const aiNodeAnim *> >::Element *F = anim_tracks.front(); F; F = F->next()) {
				_insert_pivot_anim_track(state.meshes, F->key(), F->get(), state.ap, sk, length, ticks_per_second, animation, state.bake_fps, state.path, state.scene, state.removed_bones.has(F->key()));
			}
		}
		for (Map<String, Vector<const aiNodeAnim *> >::Element *E = node_tracks.front(); E; E = E->next()) {
			if (state.removed_nodes.has(E->key())) {
				continue;
			}
			_insert_pivot_anim_track(state.meshes, E->key(), E->get(), state.ap, NULL, length, ticks_per_second, animation, state.bake_fps, state.path, state.scene, false);
		}
		for (size_t i = 0; i < anim->mNumMorphMeshChannels; i++) {
			const aiMeshMorphAnim *anim_mesh = anim->mMorphMeshChannels[i];
			const String prop_name = _assimp_string_to_string(anim_mesh->mName);
			const String mesh_name = prop_name.split("*")[0];
			if (state.removed_nodes.has(mesh_name)) {
				continue;
			}
			ERR_CONTINUE(prop_name.split("*").size() != 2);
			const MeshInstance *mesh_instance = Object::cast_to<MeshInstance>(state.ap->get_owner()->find_node(mesh_name));
			ERR_CONTINUE(mesh_instance == NULL);
			if (state.ap->get_owner()->find_node(mesh_instance->get_name()) == NULL) {
				print_verbose("Can't find mesh in scene: " + mesh_instance->get_name());
				continue;
			}
			const String path = state.ap->get_owner()->get_path_to(mesh_instance);
			if (path.empty()) {
				print_verbose("Can't find mesh in scene");
				continue;
			}
			Ref<Mesh> mesh = mesh_instance->get_mesh();
			ERR_CONTINUE(mesh.is_null());

			float base_weight = 0.0f;
			if (anim_mesh->mNumKeys != 0) {
				if (anim_mesh->mKeys[0].mNumValuesAndWeights) {
					base_weight = anim_mesh->mKeys[0].mWeights[0];
				}
			}

			struct Blend {
				Vector<float> blend_weights;
				Vector<float> blend_times;
			};
			Map<int32_t, Blend> track_blends;
			for (size_t k = 0; k < anim_mesh->mNumKeys; k++) {
				for (size_t j = 0; j < anim_mesh->mKeys[k].mNumValuesAndWeights; j++) {

					const String prop = "blend_shapes/" + mesh->get_blend_shape_name(anim_mesh->mKeys[k].mValues[j]);
					const NodePath node_path = String(path) + ":" + prop;
					ERR_CONTINUE(state.ap->get_owner()->has_node(node_path) == false);
					int32_t blend_track_idx = animation->find_track(node_path);
					if (animation->find_track(node_path) == -1) {
						blend_track_idx = animation->get_track_count();
						animation->add_track(Animation::TYPE_VALUE);
						animation->track_set_interpolation_type(blend_track_idx, Animation::INTERPOLATION_LINEAR);
						animation->track_set_path(blend_track_idx, node_path);
						Blend blend = Blend();
						track_blends.insert(blend_track_idx, blend);
					}
					Blend &blend = track_blends.find(blend_track_idx)->get();
					blend.blend_weights.push_back(anim_mesh->mKeys[k].mWeights[j]);
					blend.blend_times.push_back(anim_mesh->mKeys[k].mTime / ticks_per_second);
				}
			}
			for (Map<int32_t, Blend>::Element *E = track_blends.front(); E; E = E->next()) {
				float increment = 1.0 / float(state.bake_fps);
				float time = 0.0;
				bool last = false;
				while (true) {
					float weight = base_weight;

					if (E->get().blend_weights.size()) {
						weight = _interpolate_track<float>(E->get().blend_times, E->get().blend_weights, time, AssetImportAnimation::INTERP_LINEAR);
					}
					animation->track_insert_key(E->key(), time, weight);

					if (last) {
						break;
					}
					time += increment;
					if (time >= length) {
						last = true;
						time = length;
					}
				}
			}
		}
	}
	animation->set_length(length);
	if (animation->get_track_count()) {
		state.ap->add_animation(name, animation);
	}
}

void EditorSceneImporterAssimp::_insert_pivot_anim_track(const Vector<MeshInstance *> p_meshes, const String p_node_name, Vector<const aiNodeAnim *> F, AnimationPlayer *ap, Skeleton *sk, float &length, float ticks_per_second, Ref<Animation> animation, int p_bake_fps, const String &p_path, const aiScene *p_scene, bool p_is_filled_bone) {
	NodePath node_path;
	if (sk != NULL) {
		const String path = ap->get_owner()->get_path_to(sk);
		if (path.empty()) {
			return;
		}
		if (sk->find_bone(p_node_name) == -1) {
			return;
		}
		node_path = path + ":" + p_node_name;
	} else {
		Node *node = ap->get_owner()->find_node(p_node_name);
		if (node == NULL) {
			return;
		}
		const String path = ap->get_owner()->get_path_to(node);
		node_path = path;
	}
	if (node_path.is_empty()) {
		return;
	}

	Vector<Vector3> pos_values;
	Vector<float> pos_times;
	Vector<Vector3> scale_values;
	Vector<float> scale_times;
	Vector<Quat> rot_values;
	Vector<float> rot_times;
	Vector3 base_pos;
	Quat base_rot;
	Vector3 base_scale = Vector3(1, 1, 1);
	bool is_translation = false;
	bool is_rotation = false;
	bool is_scaling = false;
	for (int32_t k = 0; k < F.size(); k++) {
		String p_track_type = _assimp_string_to_string(F[k]->mNodeName).split(ASSIMP_FBX_KEY)[1];
		if (p_track_type == "_Translation") {
			is_translation = is_translation || true;
		} else if (p_track_type == "_Rotation") {
			is_rotation = is_rotation || true;
		} else if (p_track_type == "_Scaling") {
			is_scaling = is_scaling || true;
		} else {
			continue;
		}
		ERR_CONTINUE(ap->get_owner()->has_node(node_path) == false);

		if (F[k]->mNumRotationKeys || F[k]->mNumPositionKeys || F[k]->mNumScalingKeys) {
			
			if (is_translation) {
				for (size_t p = 0; p < F[k]->mNumPositionKeys; p++) {
					aiVector3D pos = F[k]->mPositionKeys[p].mValue;
					pos_values.push_back(Vector3(pos.x, pos.y, pos.z));
					pos_times.push_back(F[k]->mPositionKeys[p].mTime / ticks_per_second);
				}
			}

			if (is_rotation) {
				for (size_t r = 0; r < F[k]->mNumRotationKeys; r++) {
					aiQuaternion quat = F[k]->mRotationKeys[r].mValue;
					rot_values.push_back(Quat(quat.x, quat.y, quat.z, quat.w).normalized());
					rot_times.push_back(F[k]->mRotationKeys[r].mTime / ticks_per_second);
				}
			}

			if (is_scaling) {
				for (size_t sc = 0; sc < F[k]->mNumScalingKeys; sc++) {
					aiVector3D scale = F[k]->mScalingKeys[sc].mValue;
					scale_values.push_back(Vector3(scale.x, scale.y, scale.z));
					scale_times.push_back(F[k]->mScalingKeys[sc].mTime / ticks_per_second);
				}
			}
		}
	}
	int32_t track_idx = animation->get_track_count();
	animation->add_track(Animation::TYPE_TRANSFORM);
	animation->track_set_path(track_idx, node_path);
	float increment = 1.0 / float(p_bake_fps);
	float time = 0.0;
	bool last = false;
	while (true) {
		Vector3 pos = Vector3();
		Quat rot = Quat();
		Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
		if (pos_values.size()) {
			pos = _interpolate_track<Vector3>(pos_times, pos_values, time, AssetImportAnimation::INTERP_LINEAR);
		}
		if (rot_values.size()) {
			rot = _interpolate_track<Quat>(rot_times, rot_values, time, AssetImportAnimation::INTERP_LINEAR).normalized();
		}
		if (scale_values.size()) {
			scale = _interpolate_track<Vector3>(scale_times, scale_values, time, AssetImportAnimation::INTERP_LINEAR);
		}
		animation->track_set_interpolation_type(track_idx, Animation::INTERPOLATION_LINEAR);
		animation->transform_track_insert_key(track_idx, time, pos, rot, scale);

		if (last) {
			break;
		}
		time += increment;
		if (time >= length) {
			last = true;
			time = length;
		}
	}
}

float EditorSceneImporterAssimp::_get_fbx_fps(int32_t time_mode, const aiScene *p_scene) {
	switch (time_mode) {
		case AssetImportFbx::TIME_MODE_DEFAULT: return 24; //hack
		case AssetImportFbx::TIME_MODE_120: return 120;
		case AssetImportFbx::TIME_MODE_100: return 100;
		case AssetImportFbx::TIME_MODE_60: return 60;
		case AssetImportFbx::TIME_MODE_50: return 50;
		case AssetImportFbx::TIME_MODE_48: return 48;
		case AssetImportFbx::TIME_MODE_30: return 30;
		case AssetImportFbx::TIME_MODE_30_DROP: return 30;
		case AssetImportFbx::TIME_MODE_NTSC_DROP_FRAME: return 29.9700262f;
		case AssetImportFbx::TIME_MODE_NTSC_FULL_FRAME: return 29.9700262f;
		case AssetImportFbx::TIME_MODE_PAL: return 25;
		case AssetImportFbx::TIME_MODE_CINEMA: return 24;
		case AssetImportFbx::TIME_MODE_1000: return 1000;
		case AssetImportFbx::TIME_MODE_CINEMA_ND: return 23.976f;
		case AssetImportFbx::TIME_MODE_CUSTOM:
			int32_t frame_rate;
			p_scene->mMetaData->Get("FrameRate", frame_rate);
			return frame_rate;
	}
	return 0;
}

Transform EditorSceneImporterAssimp::_get_global_ai_node_transform(const aiScene *p_scene, const aiNode *p_current_node) {
	aiNode const *current_node = p_current_node;
	Transform xform;
	while (current_node != NULL) {
		xform = _ai_matrix_transform(current_node->mTransformation) * xform;
		current_node = current_node->mParent;
	}
	return xform;
}

void EditorSceneImporterAssimp::_generate_node_bone(const aiScene *p_scene, const aiNode *p_node, Map<String, bool> &p_mesh_bones, Skeleton *p_skeleton, const String p_path, const int32_t p_max_bone_weights) {
	for (size_t i = 0; i < p_node->mNumMeshes; i++) {
		const unsigned int mesh_idx = p_node->mMeshes[i];
		const aiMesh *ai_mesh = p_scene->mMeshes[mesh_idx];
		for (size_t j = 0; j < ai_mesh->mNumBones; j++) {
			String bone_name = _assimp_string_to_string(ai_mesh->mBones[j]->mName);
			if (p_skeleton->find_bone(bone_name) != -1) {
				continue;
			}
			if (bone_name.split(ASSIMP_FBX_KEY).size() != 1) {
				continue;
			}
			p_mesh_bones.insert(bone_name, true);
			p_skeleton->add_bone(bone_name);
			int32_t idx = p_skeleton->find_bone(bone_name);
			Transform xform = _ai_matrix_transform(ai_mesh->mBones[j]->mOffsetMatrix);
			String ext = p_path.get_file().get_extension().to_lower();
			if (ext == "fbx") {
				Transform mesh_xform = _get_global_ai_node_transform(p_scene, p_node);
				mesh_xform.basis = Basis();
				xform = mesh_xform.affine_inverse() * xform;
			}
			p_skeleton->set_bone_rest(idx, xform.affine_inverse());
		}
	}
}

void EditorSceneImporterAssimp::_keep_node(const String &p_path, Node *p_current, Node *p_owner, Set<Node *> &r_keep_nodes) {
	if (p_current == p_owner) {
		r_keep_nodes.insert(p_current);
	}

	if (p_current->get_class() != Spatial().get_class()) {
		r_keep_nodes.insert(p_current);
	}

	for (int i = 0; i < p_current->get_child_count(); i++) {
		_keep_node(p_path, p_current->get_child(i), p_owner, r_keep_nodes);
	}
}

void EditorSceneImporterAssimp::_filter_node(const String &p_path, Node *p_current, Node *p_owner, const Set<Node *> p_keep_nodes, Set<String> &r_removed_nodes) {
	if (p_keep_nodes.has(p_current) == false) {
		r_removed_nodes.insert(p_current->get_name());
		p_current->queue_delete();
	}
	for (int i = 0; i < p_current->get_child_count(); i++) {
		_filter_node(p_path, p_current->get_child(i), p_owner, p_keep_nodes, r_removed_nodes);
	}
}

void EditorSceneImporterAssimp::_generate_node(State &state, const aiNode *p_node, Node *p_parent, Node *p_owner) {
	Spatial *child_node = NULL;
	if (p_node == NULL) {
		return;
	}
	String node_name = _assimp_string_to_string(p_node->mName);
	String ext = state.path.get_file().get_extension().to_lower();
	{
		Transform xform = _ai_matrix_transform(p_node->mTransformation);

		child_node = memnew(Spatial);
		p_parent->add_child(child_node);
		child_node->set_owner(p_owner);
		child_node->set_transform(xform);
	}

	if (p_node->mNumMeshes > 0) {
		MeshInstance *mesh_node = memnew(MeshInstance);
		p_parent->add_child(mesh_node);
		mesh_node->set_owner(p_owner);
		if (state.skeleton->get_bone_count() == 0) {
			if (mesh_node->get_parent() == state.root) {
				const aiNode *ai_mesh_node = _assimp_find_node(state.scene->mRootNode, _assimp_string_to_string(p_node->mName));
				Transform mesh_xform;
				if (ai_mesh_node) {
					mesh_xform = _get_global_ai_node_transform(state.scene, ai_mesh_node);
				}
				ai_mesh_node = _assimp_find_node(state.scene->mRootNode, _assimp_string_to_string(p_node->mName));
				state.skeleton->set_transform(mesh_xform);
			}
		}
		{
			Map<String, bool> mesh_bones;
			state.skeleton->set_use_bones_in_world_transform(true);
			_generate_node_bone(state.scene, p_node, mesh_bones, state.skeleton, state.path, state.max_bone_weights);
			Set<String> tracks;
			_get_track_set(state.scene, tracks);
			MeshInstance *mi = Object::cast_to<MeshInstance>(mesh_node);
			if (mi) {
				state.meshes.push_back(mi);
			}
			_add_mesh_to_mesh_instance(state, p_node, mesh_node, p_owner, child_node->get_transform());
		}
		if (state.skeleton->get_bone_count() > 0) {
			aiNode *skeleton_root = NULL;
			_set_bone_parent(state.skeleton, state.scene);
			for (int32_t i = 0; i < state.skeleton->get_bone_count(); i++) {
				if (state.skeleton->get_bone_parent(i) == -1) {
					skeleton_root = _assimp_find_node(state.scene->mRootNode, state.skeleton->get_bone_name(i));
					break;
				}
			}
			for (int32_t i = 0; i < state.skeleton->get_bone_count(); i++) {
				aiNode *node = _assimp_find_node(state.scene->mRootNode, state.skeleton->get_bone_name(i));
				while (node != NULL) {
					int32_t bone = state.skeleton->find_bone(_assimp_string_to_string(node->mName));
					String node_name = _assimp_string_to_string(node->mName);
					if (!node_name.empty()) {
						if (node == _assimp_find_node(state.scene->mRootNode, _assimp_string_to_string(skeleton_root->mName).split(ASSIMP_FBX_KEY)[0])) {
							break;
						}
						if (state.skeleton->find_bone(node_name) == -1 && node_name.split(ASSIMP_FBX_KEY).size() == 1) {
							state.skeleton->add_bone(node_name);
							int32_t idx = state.skeleton->find_bone(node_name);
							Transform xform = _get_global_ai_node_transform(state.scene, _assimp_find_node(state.scene->mRootNode, node_name));
							xform = _format_rot_xform(state.path, state.scene) * xform;
							state.skeleton->set_bone_rest(idx, xform);
							break;
						}
					}
					if (skeleton_root == node) {
						break;
					}
					node = node->mParent;
				}
			}
			_set_bone_parent(state.skeleton, state.scene);
			bool is_pivoted = p_owner->find_node("*" + ASSIMP_FBX_KEY + "*");
			state.skeletons.insert(state.skeleton, mesh_node);
			state.mesh_skeletons.insert(mesh_node, state.skeleton);

		} else if (state.skeleton->get_bone_count() == 0) {
			Transform xform = child_node->get_transform();
			mesh_node->set_transform(xform);
		}
		child_node->get_parent()->remove_child(child_node);
		memdelete(child_node);
		child_node = mesh_node;
	} else if (state.light_names.has(node_name)) {
		Spatial *light_node = Object::cast_to<Light>(p_owner->find_node(node_name));
		ERR_FAIL_COND(light_node == NULL);
		if (!p_parent->has_node(light_node->get_path())) {
			p_parent->add_child(light_node);
		}
		light_node->set_owner(p_owner);
		light_node->set_transform(child_node->get_transform() *
								  light_node->get_transform());
		child_node->get_parent()->remove_child(child_node);
		memdelete(child_node);
		child_node = light_node;
	} else if (state.camera_names.has(node_name)) {
		Spatial *camera_node = Object::cast_to<Camera>(p_owner->find_node(node_name));
		ERR_FAIL_COND(camera_node == NULL);
		if (!p_parent->has_node(camera_node->get_path())) {
			p_parent->add_child(camera_node);
		}
		camera_node->set_owner(p_owner);
		camera_node->set_transform(child_node->get_transform() *
								   camera_node->get_transform());
		child_node->get_parent()->remove_child(child_node);
		memdelete(child_node);
		child_node = camera_node;
	}
	child_node->set_name(node_name);
	for (size_t i = 0; i < p_node->mNumChildren; i++) {
		_generate_node(state, p_node->mChildren[i], child_node, p_owner);
	}
}

void EditorSceneImporterAssimp::_set_bone_parent(Skeleton *p_skeleton, const aiScene *p_scene) {
	for (int32_t i = 0; i < p_skeleton->get_bone_count(); i++) {
		aiNode *node = _assimp_find_node(p_scene->mRootNode, p_skeleton->get_bone_name(i));
		if (node == NULL) {
			continue;
		}
		node = node->mParent;
		while (node != NULL) {
			int32_t bone = p_skeleton->find_bone(_assimp_string_to_string(node->mName));
			if (bone != -1) {
				p_skeleton->set_bone_parent(i, bone);
				break;
			}
			node = node->mParent;
		}
	}
}

aiNode *EditorSceneImporterAssimp::_assimp_find_node(aiNode *ai_child_node, const String bone_name_mask) {

	if (_assimp_string_to_string(ai_child_node->mName).match(bone_name_mask)) {
		return ai_child_node;
	}
	aiNode *target = NULL;
	for (size_t i = 0; i < ai_child_node->mNumChildren; i++) {

		target = _assimp_find_node(ai_child_node->mChildren[i], bone_name_mask);
		if (target != NULL) {
			return target;
		}
	}
	return target;
}

Transform EditorSceneImporterAssimp::_format_rot_xform(const String p_path, const aiScene *p_scene) {
	String ext = p_path.get_file().get_extension().to_lower();

	Transform xform;
	int32_t up_axis = 0;
	Vector3 up_axis_vec3 = Vector3();

	int32_t front_axis = 0;
	Vector3 front_axis_vec3 = Vector3();

	if (p_scene->mMetaData != NULL) {
		p_scene->mMetaData->Get("UpAxis", up_axis);
		if (up_axis == AssetImportFbx::UP_VECTOR_AXIS_X) {
			if (p_scene->mMetaData != NULL) {
				p_scene->mMetaData->Get("FrontAxis", front_axis);
				if (front_axis == AssetImportFbx::FRONT_PARITY_EVEN) {
					// y
				} else if (front_axis == AssetImportFbx::FRONT_PARITY_ODD) {
					// z
					//front_axis_vec3 = Vector3(0.0f, Math::deg2rad(-180.f), 0.0f);
				}
			}
		} else if (up_axis == AssetImportFbx::UP_VECTOR_AXIS_Y) {
			up_axis_vec3 = Vector3(Math::deg2rad(-90.f), 0.0f, 0.0f);
			if (p_scene->mMetaData != NULL) {
				p_scene->mMetaData->Get("FrontAxis", front_axis);
				if (front_axis == AssetImportFbx::FRONT_PARITY_EVEN) {
					// x
				} else if (front_axis == AssetImportFbx::FRONT_PARITY_ODD) {
					// z
				}
			}
		} else if (up_axis == AssetImportFbx::UP_VECTOR_AXIS_Z) {
			up_axis_vec3 = Vector3(0.0f, Math ::deg2rad(90.f), 0.0f);
			if (p_scene->mMetaData != NULL) {
				p_scene->mMetaData->Get("FrontAxis", front_axis);
				if (front_axis == AssetImportFbx::FRONT_PARITY_EVEN) {
					// x
				} else if (front_axis == AssetImportFbx::FRONT_PARITY_ODD) {
					// y
				}
			}
		}
	}

	int32_t up_axis_sign = 0;
	if (p_scene->mMetaData != NULL) {
		p_scene->mMetaData->Get("UpAxisSign", up_axis_sign);
		up_axis_vec3 = up_axis_vec3 * up_axis_sign;
	}

	int32_t front_axis_sign = 0;
	if (p_scene->mMetaData != NULL) {
		p_scene->mMetaData->Get("FrontAxisSign", front_axis_sign);
		front_axis_vec3 = front_axis_vec3 * front_axis_sign;
	}

	int32_t coord_axis = 0;
	Vector3 coord_axis_vec3 = Vector3();
	if (p_scene->mMetaData != NULL) {
		p_scene->mMetaData->Get("CoordAxis", coord_axis);
		if (coord_axis == AssetImportFbx::COORD_LEFT) {
		} else if (coord_axis == AssetImportFbx::COORD_RIGHT) {
		}
	}

	int32_t coord_axis_sign = 0;
	if (p_scene->mMetaData != NULL) {
		p_scene->mMetaData->Get("CoordAxisSign", coord_axis_sign);
	}

	Quat up_quat;
	up_quat.set_euler(up_axis_vec3);

	Quat coord_quat;
	coord_quat.set_euler(coord_axis_vec3);

	Quat front_quat;
	front_quat.set_euler(front_axis_vec3);

	xform.basis.set_quat(up_quat * coord_quat * front_quat);
	return xform;
}

void EditorSceneImporterAssimp::_get_track_set(const aiScene *p_scene, Set<String> &tracks) {
	for (size_t i = 0; i < p_scene->mNumAnimations; i++) {
		for (size_t j = 0; j < p_scene->mAnimations[i]->mNumChannels; j++) {
			aiString ai_name = p_scene->mAnimations[i]->mChannels[j]->mNodeName;
			String name = _assimp_string_to_string(ai_name);
			tracks.insert(name);
		}
	}
}

void EditorSceneImporterAssimp::_add_mesh_to_mesh_instance(State &state, const aiNode *p_node, MeshInstance *p_mesh_instance, Node *p_owner, Transform p_mesh_xform) {
	Ref<ArrayMesh> mesh;
	mesh.instance();
	bool has_uvs = false;
	for (size_t i = 0; i < p_node->mNumMeshes; i++) {
		const unsigned int mesh_idx = p_node->mMeshes[i];
		const aiMesh *ai_mesh = state.scene->mMeshes[mesh_idx];

		Map<uint32_t, Vector<float> > vertex_weight;
		Map<uint32_t, Vector<String> > vertex_bone_name;
		for (size_t b = 0; b < ai_mesh->mNumBones; b++) {
			aiBone *bone = ai_mesh->mBones[b];
			for (size_t w = 0; w < bone->mNumWeights; w++) {
				String name = _assimp_string_to_string(bone->mName);
				aiVertexWeight ai_weights = bone->mWeights[w];
				uint32_t vertexId = ai_weights.mVertexId;
				Map<uint32_t, Vector<float> >::Element *result = vertex_weight.find(vertexId);
				Vector<float> weights;
				if (result != NULL) {
					weights.append_array(result->value());
				}
				weights.push_back(ai_weights.mWeight);
				if (vertex_weight.has(vertexId)) {
					vertex_weight[vertexId] = weights;
				} else {
					vertex_weight.insert(vertexId, weights);
				}
				Map<uint32_t, Vector<String> >::Element *bone_result = vertex_bone_name.find(vertexId);
				Vector<String> bone_names;
				if (bone_result != NULL) {
					bone_names.append_array(bone_result->value());
				}
				bone_names.push_back(name);
				if (vertex_bone_name.has(vertexId)) {
					vertex_bone_name[vertexId] = bone_names;
				} else {
					vertex_bone_name.insert(vertexId, bone_names);
				}
			}
		}
		Ref<SurfaceTool> st;
		st.instance();
		//VisualServer::ArrayFormat format_flags = (VisualServer::ArrayFormat)0;
		//if (p_max_bone_weights == 8) {
		//	format_flags = VisualServer::ARRAY_FLAG_USE_8_WEIGHTS;
		//}
		//st->begin(Mesh::PRIMITIVE_TRIANGLES, format_flags);
		st->begin(Mesh::PRIMITIVE_TRIANGLES);
		for (size_t j = 0; j < ai_mesh->mNumVertices; j++) {
			if (ai_mesh->HasTextureCoords(0)) {
				has_uvs = true;
				st->add_uv(Vector2(ai_mesh->mTextureCoords[0][j].x, 1.0f - ai_mesh->mTextureCoords[0][j].y));
			}
			if (ai_mesh->HasTextureCoords(1)) {
				has_uvs = true;
				st->add_uv2(Vector2(ai_mesh->mTextureCoords[1][j].x, 1.0f - ai_mesh->mTextureCoords[1][j].y));
			}
			if (ai_mesh->HasVertexColors(0)) {
				Color color = Color(ai_mesh->mColors[0]->r, ai_mesh->mColors[0]->g, ai_mesh->mColors[0]->b, ai_mesh->mColors[0]->a);
				st->add_color(color);
			}
			if (ai_mesh->mNormals != NULL) {
				const aiVector3D normals = ai_mesh->mNormals[j];
				const Vector3 godot_normal = Vector3(normals.x, normals.y, normals.z);
				st->add_normal(godot_normal);
				if (ai_mesh->HasTangentsAndBitangents()) {
					const aiVector3D tangents = ai_mesh->mTangents[j];
					const Vector3 godot_tangent = Vector3(tangents.x, tangents.y, tangents.z);
					const aiVector3D bitangent = ai_mesh->mBitangents[j];
					const Vector3 godot_bitangent = Vector3(bitangent.x, bitangent.y, bitangent.z);
					float d = godot_normal.cross(godot_tangent).dot(godot_bitangent) > 0.0f ? 1.0f : -1.0f;
					st->add_tangent(Plane(tangents.x, tangents.y, tangents.z, d));
				}
			}

			if (state.skeleton != NULL && state.skeleton->get_bone_count() > 0) {
				Map<uint32_t, Vector<String> >::Element *I = vertex_bone_name.find(j);
				Vector<int32_t> bones;
				if (I != NULL) {
					Vector<String> bone_names;
					bone_names.append_array(I->value());
					for (int32_t f = 0; f < bone_names.size(); f++) {
						int32_t bone = state.skeleton->find_bone(bone_names[f]);
						ERR_EXPLAIN("Asset Importer: Mesh can't find bone " + bone_names[f]);
						ERR_FAIL_COND(bone == -1);
						bones.push_back(bone);
					}
					if (state.skeleton->get_bone_count()) {
						int32_t add = CLAMP(state.max_bone_weights - bones.size(), 0, state.max_bone_weights);
						for (int32_t f = 0; f < add; f++) {
							bones.push_back(0);
						}
					}
					st->add_bones(bones);
					Map<uint32_t, Vector<float> >::Element *E = vertex_weight.find(j);
					Vector<float> weights;
					if (E != NULL) {
						weights = E->value();
						if (weights.size() != state.max_bone_weights) {
							int32_t add = CLAMP(state.max_bone_weights - weights.size(), 0, state.max_bone_weights);
							for (int32_t f = 0; f < add; f++) {
								weights.push_back(0.0f);
							}
						}
					}
					ERR_CONTINUE(weights.size() == 0);
					st->add_weights(weights);
				}
			}
			const aiVector3D pos = ai_mesh->mVertices[j];
			Vector3 godot_pos = Vector3(pos.x, pos.y, pos.z);
			const Transform mesh_parent_global_xform = _get_global_ai_node_transform(state.scene, p_node->mParent);
			Transform mesh_xform = p_mesh_xform;
			mesh_xform = mesh_parent_global_xform.affine_inverse() * p_mesh_xform;
			if (p_mesh_instance->get_parent() != state.root) {
				bool has_pivot = mesh_xform.basis != Basis();
				if (!has_pivot) {
					mesh_xform = Transform();
				}
			} else {
				mesh_xform = _get_global_ai_node_transform(state.scene, p_node);
			}
			godot_pos = mesh_xform.xform(godot_pos);
			st->add_vertex(godot_pos);
		}
		for (size_t j = 0; j < ai_mesh->mNumFaces; j++) {
			const aiFace face = ai_mesh->mFaces[j];
			ERR_FAIL_COND(face.mNumIndices != 3);
			Vector<size_t> order;
			order.push_back(2);
			order.push_back(1);
			order.push_back(0);
			for (int32_t k = 0; k < order.size(); k++) {
				st->add_index(face.mIndices[order[k]]);
			}
		}
		if (ai_mesh->HasTangentsAndBitangents() == false && has_uvs) {
			st->generate_tangents();
		}
		aiMaterial *ai_material = state.scene->mMaterials[ai_mesh->mMaterialIndex];
		Ref<SpatialMaterial> mat;
		mat.instance();

		int32_t mat_two_sided = 0;
		if (AI_SUCCESS == ai_material->Get(AI_MATKEY_TWOSIDED, mat_two_sided)) {
			if (mat_two_sided > 0) {
				mat->set_cull_mode(SpatialMaterial::CULL_DISABLED);
			}
		}

		const String mesh_name = _assimp_string_to_string(ai_mesh->mName);
		aiString mat_name;
		if (AI_SUCCESS == ai_material->Get(AI_MATKEY_NAME, mat_name)) {
			mat->set_name(_assimp_string_to_string(mat_name));
		}

		aiTextureType tex_normal = aiTextureType_NORMALS;
		{
			aiString ai_filename = aiString();
			String filename = "";
			aiTextureMapMode map_mode[2];

			if (AI_SUCCESS == ai_material->GetTexture(tex_normal, 0, &ai_filename, NULL, NULL, NULL, NULL, map_mode)) {
				filename = _assimp_raw_string_to_string(ai_filename);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);

					if (texture != NULL) {
						if (map_mode != NULL) {
							_set_texture_mapping_mode(map_mode, texture);
						}
						mat->set_feature(SpatialMaterial::Feature::FEATURE_NORMAL_MAPPING, true);
						mat->set_texture(SpatialMaterial::TEXTURE_NORMAL, texture);
					}
				}
			}
		}

		{
			aiString tex_fbx_pbs_emissive_path;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_EMISSION_TEXTURE, tex_fbx_pbs_emissive_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_emissive_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					if (texture != NULL) {
						mat->set_feature(SpatialMaterial::FEATURE_EMISSION, true);
						mat->set_texture(SpatialMaterial::TEXTURE_EMISSION, texture);
					}
				}
			} else {
				float pbr_emission = 0.0f;
				if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_EMISSIVE_FACTOR, pbr_emission)) {
					mat->set_emission(Color(pbr_emission, pbr_emission, pbr_emission, 1.0f));
				}
			}
		}

		{
			aiString ai_filename = aiString();
			String filename = "";

			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_NORMAL_TEXTURE, ai_filename)) {
				filename = _assimp_raw_string_to_string(ai_filename);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					if (texture != NULL) {
						mat->set_feature(SpatialMaterial::Feature::FEATURE_NORMAL_MAPPING, true);
						mat->set_texture(SpatialMaterial::TEXTURE_NORMAL, texture);
					}
				}
			}
		}

		aiTextureType tex_emissive = aiTextureType_EMISSIVE;

		if (ai_material->GetTextureCount(tex_emissive) > 0) {

			aiString ai_filename = aiString();
			String filename = "";
			aiTextureMapMode map_mode[2];

			if (AI_SUCCESS == ai_material->GetTexture(tex_emissive, 0, &ai_filename, NULL, NULL, NULL, NULL, map_mode)) {
				filename = _assimp_raw_string_to_string(ai_filename);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					if (texture != NULL) {
						_set_texture_mapping_mode(map_mode, texture);
						mat->set_feature(SpatialMaterial::FEATURE_EMISSION, true);
						mat->set_texture(SpatialMaterial::TEXTURE_EMISSION, texture);
					}
				}
			}
		}

		aiTextureType tex_albedo = aiTextureType_DIFFUSE;
		if (ai_material->GetTextureCount(tex_albedo) > 0) {

			aiString ai_filename = aiString();
			String filename = "";
			aiTextureMapMode map_mode[2];
			if (AI_SUCCESS == ai_material->GetTexture(tex_albedo, 0, &ai_filename, NULL, NULL, NULL, NULL, map_mode)) {
				filename = _assimp_raw_string_to_string(ai_filename);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					if (texture != NULL) {
						if (texture->get_data()->detect_alpha() != Image::ALPHA_NONE) {
							_set_texture_mapping_mode(map_mode, texture);
							mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
							mat->set_depth_draw_mode(SpatialMaterial::DepthDrawMode::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
						}
						mat->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);
					}
				}
			}
		} else {
			aiColor4D clr_diffuse;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, clr_diffuse)) {
				if (Math::is_equal_approx(clr_diffuse.a, 1.0f) == false) {
					mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
					mat->set_depth_draw_mode(SpatialMaterial::DepthDrawMode::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
				}
				mat->set_albedo(Color(clr_diffuse.r, clr_diffuse.g, clr_diffuse.b, clr_diffuse.a));
			}
		}

		aiString tex_gltf_base_color_path = aiString();
		aiTextureMapMode map_mode[2];
		if (AI_SUCCESS == ai_material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &tex_gltf_base_color_path, NULL, NULL, NULL, NULL, map_mode)) {
			String filename = _assimp_raw_string_to_string(tex_gltf_base_color_path);
			String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
			bool found = false;
			_find_texture_path(state.path, path, found);
			if (found) {
				Ref<Texture> texture = _load_texture(state.scene, path);
				_find_texture_path(state.path, path, found);
				if (texture != NULL) {
					if (texture->get_data()->detect_alpha() == Image::ALPHA_BLEND) {
						_set_texture_mapping_mode(map_mode, texture);
						mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
						mat->set_depth_draw_mode(SpatialMaterial::DepthDrawMode::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
					}
					mat->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);
				}
			}
		} else {
			aiColor4D pbr_base_color;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, pbr_base_color)) {
				if (Math::is_equal_approx(pbr_base_color.a, 1.0f) == false) {
					mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
					mat->set_depth_draw_mode(SpatialMaterial::DepthDrawMode::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
				}
				mat->set_albedo(Color(pbr_base_color.r, pbr_base_color.g, pbr_base_color.b, pbr_base_color.a));
			}
		}
		{
			aiString tex_fbx_pbs_base_color_path = aiString();
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_BASE_COLOR_TEXTURE, tex_fbx_pbs_base_color_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_base_color_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					_find_texture_path(state.path, path, found);
					if (texture != NULL) {
						if (texture->get_data()->detect_alpha() == Image::ALPHA_BLEND) {
							mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
							mat->set_depth_draw_mode(SpatialMaterial::DepthDrawMode::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
						}
						mat->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);
					}
				}
			} else {
				aiColor4D pbr_base_color;
				if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_BASE_COLOR_FACTOR, pbr_base_color)) {
					mat->set_albedo(Color(pbr_base_color.r, pbr_base_color.g, pbr_base_color.b, pbr_base_color.a));
				}
			}

			aiUVTransform pbr_base_color_uv_xform;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_BASE_COLOR_UV_XFORM, pbr_base_color_uv_xform)) {
				mat->set_uv1_offset(Vector3(pbr_base_color_uv_xform.mTranslation.x, pbr_base_color_uv_xform.mTranslation.y, 0.0f));
				mat->set_uv1_scale(Vector3(pbr_base_color_uv_xform.mScaling.x, pbr_base_color_uv_xform.mScaling.y, 1.0f));
			}
		}

		{
			aiString tex_fbx_pbs_normal_path = aiString();
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_NORMAL_TEXTURE, tex_fbx_pbs_normal_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_normal_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					_find_texture_path(state.path, path, found);
					if (texture != NULL) {
						mat->set_feature(SpatialMaterial::Feature::FEATURE_NORMAL_MAPPING, true);
						mat->set_texture(SpatialMaterial::TEXTURE_NORMAL, texture);
					}
				}
			}
		}

		aiString cull_mode;
		if (p_node->mMetaData) {
			p_node->mMetaData->Get("Culling", cull_mode);
		}
		if (cull_mode.length != 0 && cull_mode == aiString("CullingOff")) {
			mat->set_cull_mode(SpatialMaterial::CULL_DISABLED);
		}

		{
			aiString tex_fbx_stingray_normal_path = aiString();
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_NORMAL_TEXTURE, tex_fbx_stingray_normal_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_stingray_normal_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					_find_texture_path(state.path, path, found);
					if (texture != NULL) {
						mat->set_feature(SpatialMaterial::Feature::FEATURE_NORMAL_MAPPING, true);
						mat->set_texture(SpatialMaterial::TEXTURE_NORMAL, texture);
					}
				}
			}
		}

		{
			aiString tex_fbx_pbs_base_color_path = aiString();
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_COLOR_TEXTURE, tex_fbx_pbs_base_color_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_base_color_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					_find_texture_path(state.path, path, found);
					if (texture != NULL) {
						if (texture->get_data()->detect_alpha() == Image::ALPHA_BLEND) {
							mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
							mat->set_depth_draw_mode(SpatialMaterial::DepthDrawMode::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
						}
						mat->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);
					}
				}
			} else {
				aiColor4D pbr_base_color;
				if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_BASE_COLOR_FACTOR, pbr_base_color)) {
					mat->set_albedo(Color(pbr_base_color.r, pbr_base_color.g, pbr_base_color.b, pbr_base_color.a));
				}
			}

			aiUVTransform pbr_base_color_uv_xform;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_COLOR_UV_XFORM, pbr_base_color_uv_xform)) {
				mat->set_uv1_offset(Vector3(pbr_base_color_uv_xform.mTranslation.x, pbr_base_color_uv_xform.mTranslation.y, 0.0f));
				mat->set_uv1_scale(Vector3(pbr_base_color_uv_xform.mScaling.x, pbr_base_color_uv_xform.mScaling.y, 1.0f));
			}
		}

		{
			aiString tex_fbx_pbs_emissive_path = aiString();
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_TEXTURE, tex_fbx_pbs_emissive_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_emissive_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					_find_texture_path(state.path, path, found);
					if (texture != NULL) {
						if (texture->get_data()->detect_alpha() == Image::ALPHA_BLEND) {
							mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
							mat->set_depth_draw_mode(SpatialMaterial::DepthDrawMode::DEPTH_DRAW_ALPHA_OPAQUE_PREPASS);
						}
						mat->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);
					}
				}
			} else {
				aiColor4D pbr_emmissive_color;
				if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_FACTOR, pbr_emmissive_color)) {
					mat->set_emission(Color(pbr_emmissive_color.r, pbr_emmissive_color.g, pbr_emmissive_color.b, pbr_emmissive_color.a));
				}
			}

			real_t pbr_emission_intensity;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_EMISSIVE_INTENSITY_FACTOR, pbr_emission_intensity)) {
				mat->set_emission_energy(pbr_emission_intensity);
			}
		}

		aiString tex_gltf_pbr_metallicroughness_path;
		if (AI_SUCCESS == ai_material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &tex_gltf_pbr_metallicroughness_path)) {
			String filename = _assimp_raw_string_to_string(tex_gltf_pbr_metallicroughness_path);
			String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
			bool found = false;
			_find_texture_path(state.path, path, found);
			if (found) {
				Ref<Texture> texture = _load_texture(state.scene, path);
				if (texture != NULL) {
					mat->set_texture(SpatialMaterial::TEXTURE_METALLIC, texture);
					mat->set_metallic_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_BLUE);
					mat->set_texture(SpatialMaterial::TEXTURE_ROUGHNESS, texture);
					mat->set_roughness_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_GREEN);
				}
			}
		} else {
			float pbr_roughness = 0.0f;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, pbr_roughness)) {
				mat->set_roughness(pbr_roughness);
			}
			float pbr_metallic = 0.0f;

			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, pbr_metallic)) {
				mat->set_metallic(pbr_metallic);
			}
		}
		{
			aiString tex_fbx_pbs_metallic_path;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_METALLIC_TEXTURE, tex_fbx_pbs_metallic_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_metallic_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					if (texture != NULL) {
						mat->set_texture(SpatialMaterial::TEXTURE_METALLIC, texture);
						mat->set_metallic_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_GRAYSCALE);
					}
				}
			} else {
				float pbr_metallic = 0.0f;
				if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_METALLIC_FACTOR, pbr_metallic)) {
					mat->set_metallic(pbr_metallic);
				}
			}

			aiString tex_fbx_pbs_rough_path;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_ROUGHNESS_TEXTURE, tex_fbx_pbs_rough_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_rough_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					if (texture != NULL) {
						mat->set_texture(SpatialMaterial::TEXTURE_ROUGHNESS, texture);
						mat->set_roughness_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_GRAYSCALE);
					}
				}
			} else {
				float pbr_roughness = 0.04f;

				if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_STINGRAY_ROUGHNESS_FACTOR, pbr_roughness)) {
					mat->set_roughness(pbr_roughness);
				}
			}
		}

		{
			aiString tex_fbx_pbs_metallic_path;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_METALNESS_TEXTURE, tex_fbx_pbs_metallic_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_metallic_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					if (texture != NULL) {
						mat->set_texture(SpatialMaterial::TEXTURE_METALLIC, texture);
						mat->set_metallic_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_GRAYSCALE);
					}
				}
			} else {
				float pbr_metallic = 0.0f;
				if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_METALNESS_FACTOR, pbr_metallic)) {
					mat->set_metallic(pbr_metallic);
				}
			}

			aiString tex_fbx_pbs_rough_path;
			if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_DIFFUSE_ROUGHNESS_TEXTURE, tex_fbx_pbs_rough_path)) {
				String filename = _assimp_raw_string_to_string(tex_fbx_pbs_rough_path);
				String path = state.path.get_base_dir() + "/" + filename.replace("\\", "/");
				bool found = false;
				_find_texture_path(state.path, path, found);
				if (found) {
					Ref<Texture> texture = _load_texture(state.scene, path);
					if (texture != NULL) {
						mat->set_texture(SpatialMaterial::TEXTURE_ROUGHNESS, texture);
						mat->set_roughness_texture_channel(SpatialMaterial::TEXTURE_CHANNEL_GRAYSCALE);
					}
				}
			} else {
				float pbr_roughness = 0.04f;

				if (AI_SUCCESS == ai_material->Get(AI_MATKEY_FBX_MAYA_DIFFUSE_ROUGHNESS_FACTOR, pbr_roughness)) {
					mat->set_roughness(pbr_roughness);
				}
			}
		}

		Array array_mesh = st->commit_to_arrays();
		Array morphs;
		morphs.resize(ai_mesh->mNumAnimMeshes);
		Mesh::PrimitiveType primitive = Mesh::PRIMITIVE_TRIANGLES;
		Map<uint32_t, String> morph_mesh_idx_names;
		for (size_t j = 0; j < ai_mesh->mNumAnimMeshes; j++) {

			String ai_anim_mesh_name = _assimp_string_to_string(ai_mesh->mAnimMeshes[j]->mName);
			mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_NORMALIZED);
			if (ai_anim_mesh_name.empty()) {
				ai_anim_mesh_name = String("morph_") + itos(j);
			}
			mesh->add_blend_shape(ai_anim_mesh_name);
			morph_mesh_idx_names.insert(j, ai_anim_mesh_name);
			Array array_copy;
			array_copy.resize(VisualServer::ARRAY_MAX);

			for (int l = 0; l < VisualServer::ARRAY_MAX; l++) {
				array_copy[l] = array_mesh[l].duplicate(true);
			}

			const size_t num_vertices = ai_mesh->mAnimMeshes[j]->mNumVertices;
			array_copy[Mesh::ARRAY_INDEX] = Variant();
			if (ai_mesh->mAnimMeshes[j]->HasPositions()) {
				PoolVector3Array vertices;
				vertices.resize(num_vertices);
				for (size_t l = 0; l < num_vertices; l++) {
					const aiVector3D ai_pos = ai_mesh->mAnimMeshes[j]->mVertices[l];
					Vector3 position = Vector3(ai_pos.x, ai_pos.y, ai_pos.z);
					vertices.write()[l] = position;
				}
				PoolVector3Array new_vertices = array_copy[VisualServer::ARRAY_VERTEX].duplicate(true);

				for (int32_t l = 0; l < vertices.size(); l++) {
					PoolVector3Array::Write w = new_vertices.write();
					w[l] = vertices[l];
				}
				ERR_CONTINUE(vertices.size() != new_vertices.size());
				array_copy[VisualServer::ARRAY_VERTEX] = new_vertices;
			}

			int32_t color_set = 0;
			if (ai_mesh->mAnimMeshes[j]->HasVertexColors(color_set)) {
				PoolColorArray colors;
				colors.resize(num_vertices);
				for (size_t l = 0; l < num_vertices; l++) {
					const aiColor4D ai_color = ai_mesh->mAnimMeshes[j]->mColors[color_set][l];
					Color color = Color(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
					colors.write()[l] = color;
				}
				PoolColorArray new_colors = array_copy[VisualServer::ARRAY_COLOR].duplicate(true);

				for (int32_t l = 0; l < colors.size(); l++) {
					PoolColorArray::Write w = new_colors.write();
					w[l] = colors[l];
				}
				array_copy[VisualServer::ARRAY_COLOR] = new_colors;
			}

			if (ai_mesh->mAnimMeshes[j]->HasNormals()) {
				PoolVector3Array normals;
				normals.resize(num_vertices);
				for (size_t l = 0; l < num_vertices; l++) {
					const aiVector3D ai_normal = ai_mesh->mAnimMeshes[i]->mNormals[l];
					Vector3 normal = Vector3(ai_normal.x, ai_normal.y, ai_normal.z);
					normals.write()[l] = normal;
				}
				PoolVector3Array new_normals = array_copy[VisualServer::ARRAY_NORMAL].duplicate(true);

				for (int l = 0; l < normals.size(); l++) {
					PoolVector3Array::Write w = new_normals.write();
					w[l] = normals[l];
				}
				array_copy[VisualServer::ARRAY_NORMAL] = new_normals;
			}

			if (ai_mesh->mAnimMeshes[j]->HasTangentsAndBitangents()) {
				PoolColorArray tangents;
				tangents.resize(num_vertices);
				PoolColorArray::Write w = tangents.write();
				for (size_t l = 0; l < num_vertices; l++) {
					_calc_tangent_from_mesh(ai_mesh, j, l, l, w);
				}
				PoolRealArray new_tangents = array_copy[VisualServer::ARRAY_TANGENT].duplicate(true);
				ERR_CONTINUE(new_tangents.size() != tangents.size() * 4);
				for (int32_t l = 0; l < tangents.size(); l++) {
					new_tangents.write()[l + 0] = tangents[l].r;
					new_tangents.write()[l + 1] = tangents[l].g;
					new_tangents.write()[l + 2] = tangents[l].b;
					new_tangents.write()[l + 3] = tangents[l].a;
				}

				array_copy[VisualServer::ARRAY_TANGENT] = new_tangents;
			}

			morphs[j] = array_copy;
		}
		//mesh->add_surface_from_arrays(primitive, array_mesh, morphs, format_flags);
		mesh->add_surface_from_arrays(primitive, array_mesh, morphs);
		mesh->surface_set_material(i, mat);
		mesh->surface_set_name(i, _assimp_string_to_string(ai_mesh->mName));
		state.mesh_count++;
		print_verbose(String("Open Asset Import: Created mesh (including instances) ") + _assimp_string_to_string(ai_mesh->mName) + " " + itos(state.mesh_count) + " of " + itos(state.scene->mNumMeshes));
	}
	p_mesh_instance->set_mesh(mesh);
}

Ref<Texture> EditorSceneImporterAssimp::_load_texture(const aiScene *p_scene, String p_path) {
	Vector<String> split_path = p_path.get_basename().split("*");
	if (split_path.size() == 2) {
		size_t texture_idx = split_path[1].to_int();
		ERR_FAIL_COND_V(texture_idx >= p_scene->mNumTextures, Ref<Texture>());
		aiTexture *tex = p_scene->mTextures[texture_idx];
		String filename = _assimp_raw_string_to_string(tex->mFilename);
		filename = filename.get_file();
		print_verbose("Open Asset Import: Loading embedded texture " + filename);
		if (tex->mHeight == 0) {
			if (tex->CheckFormat("png")) {
				Ref<Image> img = Image::_png_mem_loader_func((uint8_t *)tex->pcData, tex->mWidth);
				ERR_FAIL_COND_V(img.is_null(), Ref<Texture>());

				Ref<ImageTexture> t;
				t.instance();
				t->create_from_image(img);
				t->set_storage(ImageTexture::STORAGE_COMPRESS_LOSSY);
				return t;
			} else if (tex->CheckFormat("jpg")) {
				Ref<Image> img = Image::_jpg_mem_loader_func((uint8_t *)tex->pcData, tex->mWidth);
				ERR_FAIL_COND_V(img.is_null(), Ref<Texture>());
				Ref<ImageTexture> t;
				t.instance();
				t->create_from_image(img);
				t->set_storage(ImageTexture::STORAGE_COMPRESS_LOSSY);
				return t;
			} else if (tex->CheckFormat("dds")) {
				ERR_EXPLAIN("Open Asset Import: Embedded dds not implemented");
				ERR_FAIL_COND_V(true, Ref<Texture>());
				//Ref<Image> img = Image::_dds_mem_loader_func((uint8_t *)tex->pcData, tex->mWidth);
				//ERR_FAIL_COND_V(img.is_null(), Ref<Texture>());
				//Ref<ImageTexture> t;
				//t.instance();
				//t->create_from_image(img);
				//t->set_storage(ImageTexture::STORAGE_COMPRESS_LOSSY);
				//return t;
			}
		} else {
			Ref<Image> img;
			img.instance();
			PoolByteArray arr;
			uint32_t size = tex->mWidth * tex->mHeight;
			arr.resize(size);
			memcpy(arr.write().ptr(), tex->pcData, size);
			ERR_FAIL_COND_V(arr.size() % 4 != 0, Ref<Texture>());
			//ARGB8888 to RGBA8888
			for (int32_t i = 0; i < arr.size() / 4; i++) {
				arr.write().ptr()[(4 * i) + 3] = arr[(4 * i) + 0];
				arr.write().ptr()[(4 * i) + 0] = arr[(4 * i) + 1];
				arr.write().ptr()[(4 * i) + 1] = arr[(4 * i) + 2];
				arr.write().ptr()[(4 * i) + 2] = arr[(4 * i) + 3];
			}
			img->create(tex->mWidth, tex->mHeight, true, Image::FORMAT_RGBA8, arr);
			ERR_FAIL_COND_V(img.is_null(), Ref<Texture>());

			Ref<ImageTexture> t;
			t.instance();
			t->create_from_image(img);
			t->set_storage(ImageTexture::STORAGE_COMPRESS_LOSSY);
			return t;
		}
		return Ref<Texture>();
	}
	Ref<Texture> p_texture = ResourceLoader::load(p_path, "Texture");
	return p_texture;
}

void EditorSceneImporterAssimp::_calc_tangent_from_mesh(const aiMesh *ai_mesh, int i, int tri_index, int index, PoolColorArray::Write &w) {
	const aiVector3D normals = ai_mesh->mAnimMeshes[i]->mNormals[tri_index];
	const Vector3 godot_normal = Vector3(normals.x, normals.y, normals.z);
	const aiVector3D tangent = ai_mesh->mAnimMeshes[i]->mTangents[tri_index];
	const Vector3 godot_tangent = Vector3(tangent.x, tangent.y, tangent.z);
	const aiVector3D bitangent = ai_mesh->mAnimMeshes[i]->mBitangents[tri_index];
	const Vector3 godot_bitangent = Vector3(bitangent.x, bitangent.y, bitangent.z);
	float d = godot_normal.cross(godot_tangent).dot(godot_bitangent) > 0.0f ? 1.0f : -1.0f;
	Color plane_tangent = Color(tangent.x, tangent.y, tangent.z, d);
	w[index] = plane_tangent;
}

void EditorSceneImporterAssimp::_set_texture_mapping_mode(aiTextureMapMode *map_mode, Ref<Texture> texture) {
	ERR_FAIL_COND(map_mode == NULL);
	aiTextureMapMode tex_mode = aiTextureMapMode::aiTextureMapMode_Wrap;
	//for (size_t i = 0; i < 3; i++) {
	tex_mode = map_mode[0];
	//}
	int32_t flags = Texture::FLAGS_DEFAULT;
	if (tex_mode == aiTextureMapMode_Wrap) {
		//Default
	} else if (tex_mode == aiTextureMapMode_Clamp) {
		flags = flags & ~Texture::FLAG_REPEAT;
	} else if (tex_mode == aiTextureMapMode_Mirror) {
		flags = flags | Texture::FLAG_MIRRORED_REPEAT;
	}
	texture->set_flags(flags);
}

void EditorSceneImporterAssimp::_find_texture_path(const String &r_p_path, String &r_path, bool &r_found) {

	_Directory dir;

	List<String> exts;
	ImageLoader::get_recognized_extensions(&exts);

	Vector<String> split_path = r_path.get_basename().split("*");
	if (split_path.size() == 2) {
		r_found = true;
		return;
	}

	if (dir.file_exists(r_p_path.get_base_dir() + r_path.get_file())) {
		r_path = r_p_path.get_base_dir() + r_path.get_file();
		r_found = true;
		return;
	}

	for (int32_t i = 0; i < exts.size(); i++) {
		if (r_found) {
			return;
		}
		if (r_found == false) {
			_find_texture_path(r_p_path, dir, r_path, r_found, "." + exts[i]);
		}
	}
}

void EditorSceneImporterAssimp::_find_texture_path(const String &p_path, _Directory &dir, String &path, bool &found, String extension) {
	Vector<String> paths;
	paths.push_back(path.get_basename() + extension);
	paths.push_back(path + extension);
	paths.push_back(path);
	paths.push_back(p_path.get_base_dir() + "/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/" + path.get_file());
	paths.push_back(p_path.get_base_dir() + "/textures/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/textures/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/textures/" + path.get_file());
	paths.push_back(p_path.get_base_dir() + "/Textures/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/Textures/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/Textures/" + path.get_file());
	paths.push_back(p_path.get_base_dir() + "/../Textures/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/../Textures/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/../Textures/" + path.get_file());
	paths.push_back(p_path.get_base_dir() + "/../textures/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/../textures/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/../textures/" + path.get_file());
	paths.push_back(p_path.get_base_dir() + "/texture/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/texture/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/texture/" + path.get_file());
	paths.push_back(p_path.get_base_dir() + "/Texture/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/Texture/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/Texture/" + path.get_file());
	paths.push_back(p_path.get_base_dir() + "/../Texture/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/../Texture/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/../Texture/" + path.get_file());
	paths.push_back(p_path.get_base_dir() + "/../texture/" + path.get_file().get_basename() + extension);
	paths.push_back(p_path.get_base_dir() + "/../texture/" + path.get_file() + extension);
	paths.push_back(p_path.get_base_dir() + "/../texture/" + path.get_file());
	for (size_t i = 0; i < paths.size(); i++) {
		if (dir.file_exists(paths[i])) {
			found = true;
			path = paths[i];
			return;
		}
	}
}

String EditorSceneImporterAssimp::_assimp_string_to_string(const aiString p_string) const {
	Vector<char> raw_name;
	raw_name.resize(p_string.length);
	memcpy(raw_name.ptrw(), p_string.C_Str(), p_string.length);
	String name;
	name.parse_utf8(raw_name.ptrw(), raw_name.size());
	if (name.find(":") != -1) {
		String replaced_name = name.replace(":", "_");
		print_verbose("Replacing " + name + " containing : with " + replaced_name);
		name = replaced_name;
	}
	if (name.find(".") != -1) {
		String replaced_name = name.replace(".", "");
		print_verbose("Replacing " + name + " containing . with " + replaced_name);
		name = replaced_name;
	}
	return name;
}

String EditorSceneImporterAssimp::_assimp_anim_string_to_string(const aiString p_string) const {
	Vector<char> raw_name;
	raw_name.resize(p_string.length);
	memcpy(raw_name.ptrw(), p_string.C_Str(), p_string.length);
	String name;
	name.parse_utf8(p_string.C_Str() /*,p_string.length*/);
	if (name.find(":") != -1) {
		String replaced_name = name.split(":")[1];
		print_verbose("Replacing " + name + " containing : with " + replaced_name);
		name = replaced_name;
	}
	return name;
}

String EditorSceneImporterAssimp::_assimp_raw_string_to_string(const aiString p_string) const {
	Vector<char> raw_name;
	raw_name.resize(p_string.length);
	memcpy(raw_name.ptrw(), p_string.C_Str(), p_string.length);
	String name;
	name.parse_utf8(p_string.C_Str() /*,p_string.length*/);
	return name;
}

Ref<Animation> EditorSceneImporterAssimp::import_animation(const String &p_path, uint32_t p_flags, int p_bake_fps) {
	return Ref<Animation>();
}

const Transform EditorSceneImporterAssimp::_ai_matrix_transform(const aiMatrix4x4 p_matrix) {
	aiMatrix4x4 matrix = p_matrix;
	Transform xform;
	xform.set(matrix.a1, matrix.b1, matrix.c1, matrix.a2, matrix.b2, matrix.c2, matrix.a3, matrix.b3, matrix.c3, matrix.a4, matrix.b4, matrix.c4);
	xform.basis.transpose();
	xform.basis.inverse();
	//xform.set(matrix.a1, matrix.a2, matrix.a3, matrix.b1, matrix.b2, matrix.b3, matrix.c1, matrix.c2, matrix.c3, matrix.a4, matrix.b4, matrix.c4);
	return xform;
}
