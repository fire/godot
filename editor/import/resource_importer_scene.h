/*************************************************************************/
/*  resource_importer_scene.h                                            */
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

#ifndef RESOURCEIMPORTERSCENE_H
#define RESOURCEIMPORTERSCENE_H

#include "core/io/resource_importer.h"
#include "scene/3d/bone_attachment.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/skeleton.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/animation.h"
#include "scene/resources/mesh.h"
#include "scene/resources/shape.h"

class Material;

class EditorSceneImporter : public Reference {

	GDCLASS(EditorSceneImporter, Reference);

protected:
	static void _bind_methods();

	Node *import_scene_from_other_importer(const String &p_path, uint32_t p_flags, int p_bake_fps);
	Ref<Animation> import_animation_from_other_importer(const String &p_path, uint32_t p_flags, int p_bake_fps);

public:
	enum ImportFlags {
		IMPORT_SCENE = 1,
		IMPORT_ANIMATION = 2,
		IMPORT_ANIMATION_DETECT_LOOP = 4,
		IMPORT_ANIMATION_OPTIMIZE = 8,
		IMPORT_ANIMATION_FORCE_ALL_TRACKS_IN_ALL_CLIPS = 16,
		IMPORT_ANIMATION_KEEP_VALUE_TRACKS = 32,
		IMPORT_GENERATE_TANGENT_ARRAYS = 256,
		IMPORT_FAIL_ON_MISSING_DEPENDENCIES = 512,
		IMPORT_MATERIALS_IN_INSTANCES = 1024,
		IMPORT_USE_COMPRESSION = 2048

	};

	virtual uint32_t get_import_flags() const;
	virtual void get_extensions(List<String> *r_extensions) const;
	virtual Node *import_scene(const String &p_path, uint32_t p_flags, int p_bake_fps, List<String> *r_missing_deps, Error *r_err = NULL);
	virtual Ref<Animation> import_animation(const String &p_path, uint32_t p_flags, int p_bake_fps);

	EditorSceneImporter() {}
};

class EditorScenePostImport : public Reference {

	GDCLASS(EditorScenePostImport, Reference);

	String source_folder;
	String source_file;

protected:
	static void _bind_methods();

public:
	String get_source_folder() const;
	String get_source_file() const;
	virtual Node *post_import(Node *p_scene);
	virtual void init(const String &p_source_folder, const String &p_source_file);
	EditorScenePostImport();
};

class ResourceImporterScene : public ResourceImporter {
	GDCLASS(ResourceImporterScene, ResourceImporter);

	Set<Ref<EditorSceneImporter> > importers;

	static ResourceImporterScene *singleton;

	enum Presets {
		PRESET_SEPARATE_MATERIALS,
		PRESET_SEPARATE_MESHES,
		PRESET_SEPARATE_ANIMATIONS,

		PRESET_SINGLE_SCENE,

		PRESET_SEPARATE_MESHES_AND_MATERIALS,
		PRESET_SEPARATE_MESHES_AND_ANIMATIONS,
		PRESET_SEPARATE_MATERIALS_AND_ANIMATIONS,
		PRESET_SEPARATE_MESHES_MATERIALS_AND_ANIMATIONS,

		PRESET_MULTIPLE_SCENES,
		PRESET_MULTIPLE_SCENES_AND_MATERIALS,
		PRESET_MAX
	};

	enum LightBakeMode {
		LIGHT_BAKE_DISABLED,
		LIGHT_BAKE_ENABLE,
		LIGHT_BAKE_LIGHTMAPS
	};

	void _replace_owner(Node *p_node, Node *p_scene, Node *p_new_owner);
	void _mark_nodes(Node *p_current, Node *p_owner, Vector<Node *> &r_remove_nodes);
	void _clean_animation_player(Node *scene);
	void _remove_nodes(Node *node, Vector<Node *> &r_nodes);
	void _animation_player_move(Node *scene, Map<MeshInstance *, Skeleton *> &r_moved_meshes);
	void _move_nodes(Node *scene, const Map<MeshInstance *, Skeleton *> moved_meshes, const Map<BoneAttachment *, Skeleton *> moved_attachments);
	void _moved_mesh_and_attachments(Node *p_current, Node *p_owner, Map<MeshInstance *, Skeleton *> &r_moved_meshes,
			Map<BoneAttachment *, Skeleton *> &r_moved_attachments);

	void _animation_player_move(Node *scene, Map<MeshInstance *, Skeleton *> &r_moved_meshes) {
		Set<Node *> skeletons;
		for (Map<MeshInstance *, Skeleton *>::Element *E = r_moved_meshes.front(); E; E = E->next()) {
			skeletons.insert(E->get());
		}
		for (int32_t i = 0; i < scene->get_child_count(); i++) {
			AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(scene->get_child(i));
			if (!ap) {
				continue;
			}
			List<StringName> animations;
			ap->get_animation_list(&animations);
			for (List<StringName>::Element *E = animations.front(); E; E = E->next()) {
				Ref<Animation> animation = ap->get_animation(E->get());
				for (int32_t k = 0; k < animation->get_track_count(); k++) {
					NodePath path = animation->track_get_path(k);
					Node *node = scene->get_node_or_null(path);
					if (skeletons.has(node)) {
						String property;
						Vector<String> split_path = String(path).split(":");
						String name = node->get_name();
						if (split_path.size() == 2) {
							property = split_path[1];
							animation->track_set_path(k, name + ":" + property);
						} else {
							animation->track_set_path(k, name);
						}
					}
				}
			}
		}
	}
	void _move_nodes(Node *scene, const Map<MeshInstance *, Skeleton *> moved_meshes, const Map<BoneAttachment *, Skeleton *> moved_attachments) {
		Map<Skeleton *, Set<MeshInstance *> > new_meshes_location;
		for (Map<MeshInstance *, Skeleton *>::Element *E = moved_meshes.front(); E; E = E->next()) {
			if (new_meshes_location.find(E->get())) {
				Set<MeshInstance *> meshes = new_meshes_location[E->get()];
				meshes.insert(E->key());
				new_meshes_location.insert(E->get(), meshes);
			} else {
				Set<MeshInstance *> meshes;
				meshes.insert(E->key());
				new_meshes_location.insert(E->get(), meshes);
			}
			E->key()->queue_delete();
		}

		for (Map<Skeleton *, Set<MeshInstance *> >::Element *new_mesh_i = new_meshes_location.front(); new_mesh_i; new_mesh_i = new_mesh_i->next()) {
			Skeleton *skel = Object::cast_to<Skeleton>(new_mesh_i->key()->duplicate());
			scene->add_child(skel);
			skel->set_owner(scene);
			for (Set<MeshInstance *>::Element *mesh_i = new_mesh_i->get().front(); mesh_i; mesh_i = mesh_i->next()) {
				MeshInstance *mi = Object::cast_to<MeshInstance>(mesh_i->get()->duplicate());
				if(!new_mesh_i->key()->is_a_parent_of(mesh_i->get())) {
					mi->set_transform(mi->get_global_transform().affine_inverse() * mesh_i->get()->get_global_transform());				
				}
				skel->add_child(mi);
				mi->set_owner(scene);
				mi->set_skeleton_path(NodePath(".."));
			}
			for (Map<BoneAttachment *, Skeleton *>::Element *attachment_i = moved_attachments.front(); attachment_i; attachment_i = attachment_i->next()) {
				BoneAttachment *attachment = Object::cast_to<BoneAttachment>(attachment_i->get()->duplicate());
				skel->add_child(attachment);
				attachment->set_owner(scene);									
				attachment_i->get()->queue_delete();
			}
			new_mesh_i->key()->queue_delete();
		}
	}

	void _moved_mesh_and_attachments(Node *p_current, Node *p_owner, Map<MeshInstance *, Skeleton *> &r_moved_meshes,
			Map<BoneAttachment *, Skeleton *> &r_moved_attachments) {
		MeshInstance *mi = Object::cast_to<MeshInstance>(p_current);
		if (mi) {
			Skeleton *skeleton = Object::cast_to<Skeleton>(mi->get_node_or_null(mi->get_skeleton_path()));
			if (skeleton) {
				r_moved_meshes.insert(mi, skeleton);
			}
		}
		BoneAttachment *bone_attachment = Object::cast_to<BoneAttachment>(p_current);
		if (bone_attachment) {
			Skeleton *skeleton = Object::cast_to<Skeleton>(mi->get_node_or_null(mi->get_skeleton_path()));
			if (skeleton) {
				r_moved_attachments.insert(bone_attachment, skeleton);
			}
		}

		for (int i = 0; i < p_current->get_child_count(); i++) {
			_moved_mesh_and_attachments(p_current->get_child(i), p_owner, r_moved_meshes, r_moved_attachments);
		}
	}

public:
	static ResourceImporterScene *get_singleton() { return singleton; }

	const Set<Ref<EditorSceneImporter> > &get_importers() const { return importers; }

	void add_importer(Ref<EditorSceneImporter> p_importer) { importers.insert(p_importer); }
	void remove_importer(Ref<EditorSceneImporter> p_importer) { importers.erase(p_importer); }

	virtual String get_importer_name() const;
	virtual String get_visible_name() const;
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual String get_save_extension() const;
	virtual String get_resource_type() const;

	virtual int get_preset_count() const;
	virtual String get_preset_name(int p_idx) const;

	virtual void get_import_options(List<ImportOption> *r_options, int p_preset = 0) const;
	virtual bool get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const;
	virtual int get_import_order() const { return 100; } //after everything

	void _find_meshes(Node *p_node, Map<Ref<ArrayMesh>, Transform> &meshes);

	void _make_external_resources(Node *p_node, const String &p_base_path, bool p_make_animations, bool p_animations_as_text, bool p_keep_animations, bool p_make_materials, bool p_materials_as_text, bool p_keep_materials, bool p_make_meshes, bool p_meshes_as_text, Map<Ref<Animation>, Ref<Animation> > &p_animations, Map<Ref<Material>, Ref<Material> > &p_materials, Map<Ref<ArrayMesh>, Ref<ArrayMesh> > &p_meshes);

	Node *_fix_node(Node *p_node, Node *p_root, Map<Ref<Mesh>, List<Ref<Shape> > > &collision_map, LightBakeMode p_light_bake_mode);

	void _create_clips(Node *scene, const Array &p_clips, bool p_bake_all);
	void _filter_anim_tracks(Ref<Animation> anim, Set<String> &keep);
	void _filter_tracks(Node *scene, const String &p_text);

	void _remove_empty_spatials(Node *scene);
	void _optimize_animations(Node *scene, float p_max_lin_error, float p_max_ang_error, float p_max_angle);

	virtual Error import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = NULL, Variant *r_metadata = NULL);

	Node *import_scene_from_other_importer(EditorSceneImporter *p_exception, const String &p_path, uint32_t p_flags, int p_bake_fps);
	Ref<Animation> import_animation_from_other_importer(EditorSceneImporter *p_exception, const String &p_path, uint32_t p_flags, int p_bake_fps);

	ResourceImporterScene();
};

class EditorSceneImporterESCN : public EditorSceneImporter {
	GDCLASS(EditorSceneImporterESCN, EditorSceneImporter);

public:
	virtual uint32_t get_import_flags() const;
	virtual void get_extensions(List<String> *r_extensions) const;
	virtual Node *import_scene(const String &p_path, uint32_t p_flags, int p_bake_fps, List<String> *r_missing_deps, Error *r_err = NULL);
	virtual Ref<Animation> import_animation(const String &p_path, uint32_t p_flags, int p_bake_fps);
};

#endif // RESOURCEIMPORTERSCENE_H
