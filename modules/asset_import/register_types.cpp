/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "register_types.h"

#include "editor/import/editor_import_collada.h"
#include "editor/import/editor_scene_importer_gltf.h"
#include "editor/import/resource_importer_obj.h"
#include "editor/import/resource_importer_scene.h"
#include "editor_scene_importer_asset_import.h"
void register_asset_import_types() {

#ifdef TOOLS_ENABLED
	Ref<ResourceImporterScene> import_scene;
	import_scene.instance();
	ResourceFormatImporter::get_singleton()->add_importer(import_scene);
	{
		Ref<EditorSceneImporterAssetImport> import_asset_import;
		import_asset_import.instance();
		import_scene->add_importer(import_asset_import);
		//TODO(Ernest) Fix hack
		{
			Ref<EditorSceneImporterCollada> import_collada;
			import_collada.instance();
			import_scene->add_importer(import_collada);

			Ref<EditorOBJImporter> import_obj2;
			import_obj2.instance();
			import_scene->add_importer(import_obj2);

			Ref<EditorSceneImporterGLTF> import_gltf;
			import_gltf.instance();
			import_scene->add_importer(import_gltf);

			Ref<EditorSceneImporterESCN> import_escn;
			import_escn.instance();
			import_scene->add_importer(import_escn);
		}
	}
#endif
}

void unregister_asset_import_types() {
}
