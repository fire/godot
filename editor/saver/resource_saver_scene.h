/*************************************************************************/
/*  resource_export_scene.h                                              */
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

#ifndef ResourceSaverScene_H
#define ResourceSaverScene_H

#include "scene/resources/animation.h"
#include "scene/resources/mesh.h"
#include "scene/resources/shape.h"
#include "core/io/resource_exporter.h"

class Material;

class EditorSceneExporter : public Reference {

	GDCLASS(EditorSceneExporter, Reference);

protected:
	static void _bind_methods();

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

	virtual uint32_t get_save_flags() const;
	virtual void get_extensions(List<String> *r_extensions) const;
	virtual void save_scene(const Node *p_node, const String &p_path, const String &p_src_path, uint32_t p_flags, int p_bake_fps, List<String> *r_missing_deps, Error *r_err = NULL);
	virtual void save_animation(const Node *p_node, const String &p_path, const String &p_src_path, uint32_t p_flags, int p_bake_fps, List<String> *r_missing_deps, Error *r_err = NULL);

	EditorSceneExporter() {}
};

class ResourceExporterScene : public ResourceExporter {
	GDCLASS(ResourceExporterScene, ResourceExporter);

	Set<Ref<EditorSceneExporter> > exporters;

	static ResourceExporterScene *singleton;

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

public:
	static ResourceExporterScene *get_singleton() { return singleton; }

	const Set<Ref<EditorSceneExporter> > &get_savers() const { return exporters; }

	void add_exporter(Ref<EditorSceneExporter> p_exporter) { exporters.insert(p_exporter); }
	void remove_exporter(Ref<EditorSceneExporter> p_exporter) { exporters.erase(p_exporter); }

	virtual String get_exporter_name() const;
	virtual String get_visible_name() const;
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual String get_save_extension() const;
	virtual String get_resource_type() const;

	virtual int get_preset_count() const;
	virtual String get_preset_name(int p_idx) const;

	virtual void get_export_options(List<ImportOption> *r_options, int p_preset = 0) const;
	virtual bool get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const;
	virtual int get_import_order() const { return 100; } //after everything

	void _find_meshes(Node *p_node, Map<Ref<ArrayMesh>, Transform> &meshes);

	void _make_external_resources(Node *p_node, const String &p_base_path, bool p_make_animations, bool p_animations_as_text, bool p_keep_animations, bool p_make_materials, bool p_materials_as_text, bool p_keep_materials, bool p_make_meshes, bool p_meshes_as_text, Map<Ref<Animation>, Ref<Animation> > &p_animations, Map<Ref<Material>, Ref<Material> > &p_materials, Map<Ref<ArrayMesh>, Ref<ArrayMesh> > &p_meshes);

	Node *_fix_node(Node *p_node, Node *p_root, Map<Ref<Mesh>, List<Ref<Shape> > > &collision_map, LightBakeMode p_light_bake_mode);

	void _create_clips(Node *scene, const Array &p_clips, bool p_bake_all);
	void _filter_anim_tracks(Ref<Animation> anim, Set<String> &keep);
	void _filter_tracks(Node *scene, const String &p_text);
	void _optimize_animations(Node *scene, float p_max_lin_error, float p_max_ang_error, float p_max_angle);
	virtual Error export_(const Node* p_node, const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = NULL, Variant *r_metadata = NULL);
	
	ResourceExporterScene();
	~ResourceExporterScene() {}
};

#endif // ResourceSaverScene_H
