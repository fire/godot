/*************************************************************************/
/*  gltf_document.h                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#ifndef GLTF_DOCUMENT_H
#define GLTF_DOCUMENT_H

#include "editor/import/resource_importer_scene.h"
#include "scene/3d/skeleton.h"
#include "scene/3d/spatial.h"
#include "scene/resources/material.h"
#include "scene/resources/texture.h"

class AnimationPlayer;
class BoneAttachment;
class MeshInstance;
class GLTFDocument : public Resource {
	GDCLASS(GLTFDocument, Resource);

public:
	typedef int GLTFAccessorIndex;
	typedef int GLTFAnimationIndex;
	typedef int GLTFBufferIndex;
	typedef int GLTFBufferViewIndex;
	typedef int GLTFCameraIndex;
	typedef int GLTFImageIndex;
	typedef int GLTFMaterialIndex;
	typedef int GLTFMeshIndex;
	typedef int GLTFNodeIndex;
	typedef int GLTFSkeletonIndex;
	typedef int GLTFSkinIndex;
	typedef int GLTFTextureIndex;

	enum GLTFType {
		TYPE_SCALAR,
		TYPE_VEC2,
		TYPE_VEC3,
		TYPE_VEC4,
		TYPE_MAT2,
		TYPE_MAT3,
		TYPE_MAT4,
	};
	struct GLTFNode {

		//matrices need to be transformed to this
		GLTFNodeIndex parent;
		int height;

		Transform xform;
		String name;

		GLTFMeshIndex mesh;
		GLTFCameraIndex camera;
		GLTFSkinIndex skin;

		GLTFSkeletonIndex skeleton;
		bool joint;

		Vector3 translation;
		Quat rotation;
		Vector3 scale;

		Vector<int> children;

		GLTFNodeIndex fake_joint_parent;

		GLTFNode() :
				parent(-1),
				height(-1),
				mesh(-1),
				camera(-1),
				skin(-1),
				skeleton(-1),
				joint(false),
				translation(0, 0, 0),
				scale(Vector3(1, 1, 1)),
				fake_joint_parent(-1) {}
	};

	struct GLTFBufferView {

		GLTFBufferIndex buffer;
		int byte_offset;
		int byte_length;
		int byte_stride;
		bool indices;
		//matrices need to be transformed to this

		GLTFBufferView() :
				buffer(-1),
				byte_offset(0),
				byte_length(0),
				byte_stride(-1),
				indices(false) {
		}
	};

	struct GLTFAccessor {

		GLTFBufferViewIndex buffer_view;
		int byte_offset;
		int component_type;
		bool normalized;
		int count;
		GLTFType type;
		Vector<real_t> min;
		Vector<real_t> max;
		int sparse_count;
		int sparse_indices_buffer_view;
		int sparse_indices_byte_offset;
		int sparse_indices_component_type;
		int sparse_values_buffer_view;
		int sparse_values_byte_offset;

		GLTFAccessor() {
			buffer_view = 0;
			byte_offset = 0;
			component_type = 0;
			normalized = false;
			count = 0;
			sparse_count = 0;
			sparse_indices_buffer_view = 0;
			sparse_indices_byte_offset = 0;
			sparse_indices_component_type = 0;
			sparse_values_buffer_view = 0;
			sparse_values_byte_offset = 0;
		}
	};
	struct GLTFTexture {
		GLTFImageIndex src_image;
	};

	struct GLTFSkeleton {
		// The *synthesized* skeletons joints
		Vector<GLTFNodeIndex> joints;

		// The roots of the skeleton. If there are multiple, each root must have the same parent
		// (ie roots are siblings)
		Vector<GLTFNodeIndex> roots;

		// The created Skeleton for the scene
		Skeleton *godot_skeleton;

		// Set of unique bone names for the skeleton
		Set<String> unique_names;

		Map<int32_t, GLTFNodeIndex> godot_bone_node;

		Vector<BoneAttachment *> bone_attachments;

		GLTFSkeleton() :
				godot_skeleton(nullptr) {
		}
	};

	struct GLTFSkin {
		String name;

		// The "skeleton" property defined in the gltf spec. -1 = Scene Root
		GLTFNodeIndex skin_root;

		Vector<GLTFNodeIndex> joints_original;
		Vector<Transform> inverse_binds;

		// Note: joints + non_joints should form a complete subtree, or subtrees with a common parent

		// All nodes that are skins that are caught in-between the original joints
		// (inclusive of joints_original)
		Vector<GLTFNodeIndex> joints;

		// All Nodes that are caught in-between skin joint nodes, and are not defined
		// as joints by any skin
		Vector<GLTFNodeIndex> non_joints;

		// The roots of the skin. In the case of multiple roots, their parent *must*
		// be the same (the roots must be siblings)
		Vector<GLTFNodeIndex> roots;

		// The GLTF Skeleton this Skin points to (after we determine skeletons)
		GLTFSkeletonIndex skeleton;

		// A mapping from the joint indices (in the order of joints_original) to the
		// Godot Skeleton's bone_indices
		Map<int, int> joint_i_to_bone_i;
		Map<int, StringName> joint_i_to_name;

		// The Actual Skin that will be created as a mapping between the IBM's of this skin
		// to the generated skeleton for the mesh instances.
		Ref<Skin> godot_skin;

		GLTFSkin() :
				skin_root(-1),
				skeleton(-1) {}
	};

	struct GLTFMesh {
		Ref<Mesh> mesh;
		Vector<float> blend_weights;
	};

	struct GLTFCamera {

		bool perspective;
		float fov_size;
		float zfar;
		float znear;

		GLTFCamera() {
			perspective = true;
			fov_size = 65;
			zfar = 500;
			znear = 0.1;
		}
	};

	struct GLTFAnimation {
		bool loop = false;

		enum Interpolation {
			INTERP_LINEAR,
			INTERP_STEP,
			INTERP_CATMULLROMSPLINE,
			INTERP_CUBIC_SPLINE
		};

		template <class T>
		struct Channel {
			Interpolation interpolation;
			Vector<float> times;
			Vector<T> values;
		};

		struct Track {

			Channel<Vector3> translation_track;
			Channel<Quat> rotation_track;
			Channel<Vector3> scale_track;
			Vector<Channel<float> > weight_tracks;
		};

		String name;

		Map<int, Track> tracks;
	};

	struct GLTFState {

		Dictionary json;
		int major_version;
		int minor_version;
		Vector<uint8_t> glb_data;

		bool use_named_skin_binds;

		Vector<GLTFNode *> nodes;
		Vector<Vector<uint8_t> > buffers;
		Vector<GLTFBufferView> buffer_views;
		Vector<GLTFAccessor> accessors;

		Vector<GLTFMesh> meshes; //meshes are loaded directly, no reason not to.

		Vector<AnimationPlayer *> animation_players;
		Map<Ref<Material>, GLTFMaterialIndex> material_cache;
		Vector<Ref<Material> > materials;

		String scene_name;
		Vector<int> root_nodes;

		Vector<GLTFTexture> textures;
		Vector<Ref<Texture> > images;

		Vector<GLTFSkin> skins;
		Vector<GLTFCamera> cameras;

		Set<String> unique_names;

		Vector<GLTFSkeleton> skeletons;
		Map<GLTFSkeletonIndex, GLTFNodeIndex> skeleton_to_node;
		Vector<GLTFAnimation> animations;

		Map<GLTFNodeIndex, Node *> scene_nodes;

		~GLTFState() {
			for (int i = 0; i < nodes.size(); i++) {
				memdelete(nodes[i]);
			}
		}
	};

	enum {
		ARRAY_BUFFER = 34962,
		ELEMENT_ARRAY_BUFFER = 34963,

		TYPE_BYTE = 5120,
		TYPE_UNSIGNED_BYTE = 5121,
		TYPE_SHORT = 5122,
		TYPE_UNSIGNED_SHORT = 5123,
		TYPE_UNSIGNED_INT = 5125,
		TYPE_FLOAT = 5126,

		COMPONENT_TYPE_BYTE = 5120,
		COMPONENT_TYPE_UNSIGNED_BYTE = 5121,
		COMPONENT_TYPE_SHORT = 5122,
		COMPONENT_TYPE_UNSIGNED_SHORT = 5123,
		COMPONENT_TYPE_INT = 5125,
		COMPONENT_TYPE_FLOAT = 5126,

	};

private:
	String _get_component_type_name(const uint32_t p_component);
	int _get_component_type_size(const int component_type);
	Error _parse_scenes(GLTFState &state);
	Error _parse_nodes(GLTFState &state);
	String _get_type_name(const GLTFType p_component);
	String _get_accessor_type_name(const GLTFDocument::GLTFType p_type);
	String _sanitize_scene_name(const String &name);
	String _gen_unique_name(GLTFState &state, const String &p_name);
	String _sanitize_bone_name(const String &name);
	String _gen_unique_bone_name(GLTFState &state, const GLTFDocument::GLTFSkeletonIndex skel_i, const String &p_name);
	GLTFTextureIndex _set_texture(GLTFState &state, Ref<Texture> p_texture);
	Ref<Texture> _get_texture(GLTFState &state, const GLTFDocument::GLTFTextureIndex p_texture);
	Error _parse_json(const String &p_path, GLTFState &state);
	Error _parse_glb(const String &p_path, GLTFState &state);
	void _compute_node_heights(GLTFState &state);
	Error _parse_buffers(GLTFState &state, const String &p_base_path);
	Error _parse_buffer_views(GLTFState &state);
	GLTFType _get_type_from_str(const String &p_string);
	Error _parse_accessors(GLTFState &state);
	Error _decode_buffer_view(GLTFState &state, double *dst, const GLTFBufferViewIndex p_buffer_view, const int skip_every, const int skip_bytes, const int element_size, const int count, const GLTFType type, const int component_count, const int component_type, const int component_size, const bool normalized, const int byte_offset, const bool for_vertex);
	Vector<double> _decode_accessor(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<float> _decode_accessor_as_floats(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<int> _decode_accessor_as_ints(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<Vector2> _decode_accessor_as_vec2(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<Vector3> _decode_accessor_as_vec3(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<Color> _decode_accessor_as_color(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<Quat> _decode_accessor_as_quat(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<Transform2D> _decode_accessor_as_xform2d(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<Basis> _decode_accessor_as_basis(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Vector<Transform> _decode_accessor_as_xform(GLTFState &state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex);
	Error _parse_meshes(GLTFState &state);
	Error _serialize_textures(GLTFState &state);
	Error _serialize_images(GLTFState &state, const String &p_path);
	Error _parse_images(GLTFState &state, const String &p_base_path);
	Error _parse_textures(GLTFState &state);
	Error _parse_materials(GLTFState &state);
	GLTFNodeIndex _find_highest_node(GLTFState &state, const Vector<GLTFNodeIndex> &subset);
	bool _capture_nodes_in_skin(GLTFState &state, GLTFSkin &skin, const GLTFNodeIndex node_index);
	void _capture_nodes_for_multirooted_skin(GLTFState &state, GLTFSkin &skin);
	Error _expand_skin(GLTFState &state, GLTFSkin &skin);
	Error _verify_skin(GLTFState &state, GLTFSkin &skin);
	Error _parse_skins(GLTFState &state);
	Error _determine_skeletons(GLTFState &state);
	Error _reparent_non_joint_skeleton_subtrees(GLTFState &state, GLTFSkeleton &skeleton, const Vector<GLTFNodeIndex> &non_joints);
	Error _reparent_to_fake_joint(GLTFState &state, GLTFSkeleton &skeleton, const GLTFNodeIndex node_index);
	Error _determine_skeleton_roots(GLTFState &state, const GLTFSkeletonIndex skel_i);
	Error _create_skeletons(GLTFState &state);
	Error _map_skin_joints_indices_to_skeleton_bone_indices(GLTFState &state);
	Error _serialize_skins(GLTFState &state);
	Error _create_skins(GLTFState &state);
	bool _skins_are_same(const Ref<Skin> &skin_a, const Ref<Skin> &skin_b);
	void _remove_duplicate_skins(GLTFState &state);
	Error _serialize_cameras(GLTFState &state);
	Error _parse_cameras(GLTFState &state);
	Error _parse_animations(GLTFState &state);
	Error _serialize_animations(GLTFState &state);
	BoneAttachment *_generate_bone_attachment(GLTFState &state, Skeleton *skeleton, const GLTFNodeIndex node_index);
	MeshInstance *_generate_mesh_instance(GLTFState &state, Node *scene_parent, const GLTFNodeIndex node_index);
	Camera *_generate_camera(GLTFState &state, Node *scene_parent, const GLTFNodeIndex node_index);
	Spatial *_generate_spatial(GLTFState &state, Node *scene_parent, const GLTFNodeIndex node_index);
	void _assign_scene_names(GLTFState &state);
	template <class T>
	T _interpolate_track(const Vector<float> &p_times, const Vector<T> &p_values, const float p_time, const GLTFAnimation::Interpolation p_interp);
	GLTFAccessorIndex _encode_accessor_as_quats(GLTFState &state, const Vector<Quat> p_attribs, const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_weights(GLTFState &state, const Vector<Color> p_attribs, const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_joints(GLTFState &state, const Vector<Color> p_attribs, const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_floats(GLTFState &state, const Vector<real_t> p_attribs, const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_vec2(GLTFState &state, const Vector<Vector2> p_attribs, const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_vec3(GLTFState &state, const Vector<Vector3> p_attribs, const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_color(GLTFState &state, const Vector<Color> p_attribs, const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_ints(GLTFState &state, const Vector<int32_t> p_attribs, const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_xform(GLTFState &state, const Vector<Transform> p_attribs, const bool p_for_vertex);
	Error _encode_buffer_view(GLTFState &state, const double *src, const int count, const GLTFType type, const int component_type, const bool normalized, const int byte_offset, const bool for_vertex, GLTFBufferViewIndex &r_accessor);
	Error _encode_accessors(GLTFDocument::GLTFState &state);
	Error _encode_buffer_views(GLTFState &state);
	Error _serialize_materials(GLTFState &state);
	Error _serialize_meshes(GLTFState &state);
	Error _serialize_nodes(GLTFState &state);
	Error _serialize_scenes(GLTFState &state);
	String interpolation_to_string(const GLTFAnimation::Interpolation p_interp);
	GLTFAnimation::Track _convert_animation_track(GLTFDocument::GLTFState &state, GLTFDocument::GLTFAnimation::Track p_track, Ref<Animation> p_animation, Transform p_bone_rest, int32_t p_track_i, GLTFDocument::GLTFNodeIndex p_node_i);
	Error _encode_buffer_bins(GLTFState &state, const String &p_path);
	Error _encode_buffer_glb(GLTFState &state, const String &p_path);
	Error _serialize_bone_attachment(GLTFState &state);
	Dictionary _serialize_texture_transform_uv1(Ref<Material> p_material);
	Dictionary _serialize_texture_transform_uv2(Ref<Material> p_material);
	Error _serialize_version(GLTFState &state);
	Error _serialize_file(GLTFState &state, const String p_path);
	Error _serialize_extensions(GLTFState &state) const;

public:
	void
	_process_mesh_instances(GLTFState &state, Node *scene_root);
	void _generate_scene_node(GLTFState &state, Node *scene_parent, Spatial *scene_root, const GLTFNodeIndex node_index);
	void _import_animation(GLTFState &state, AnimationPlayer *ap, const GLTFAnimationIndex index, const int bake_fps);
	GLTFMeshIndex _convert_mesh_instance(GLTFState &state, MeshInstance *p_mesh_instance);
	void _convert_mesh_instances(GLTFState &state);
	GLTFCameraIndex _convert_camera(GLTFState &state, Camera *p_camera);
	void _convert_skeletons(GLTFState &state);
	GLTFSkeletonIndex _convert_skeleton(GLTFState &state, Skeleton *p_skeleton, GLTFNodeIndex p_node_index);
	void _convert_spatial(GLTFState &state, Spatial *p_spatial, GLTFNode *p_node);
	void _convert_scene_node(GLTFState &state, Node *_root_node, Node *p_root_node, const GLTFNodeIndex p_root_node_index, const GLTFNodeIndex p_parent_node_index);
	void _convert_animation(GLTFState &state, AnimationPlayer *ap, String p_animation_track_name);
	Error serialize(GLTFState &state, const String &p_path);
	Error parse(GLTFState *state, String p_path);
};
#endif
