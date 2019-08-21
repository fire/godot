/*************************************************************************/
/*  import_utils.h                                                       */
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

#ifndef IMPORT_UTILS_IMPORTER_ASSIMP_H
#define IMPORT_UTILS_IMPORTER_ASSIMP_H

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

#include <string>

/**
 * Assimp Utils
 * Conversion tools / glue code to convert from assimp to godot
*/
class AssimpUtils {
public:
	static void calc_tangent_from_mesh(const aiMesh *ai_mesh, int i, int tri_index, int index, PoolColorArray::Write &w) {
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

	struct AssetImportFbx {
		enum ETimeMode {
			TIME_MODE_DEFAULT = 0,
			TIME_MODE_120 = 1,
			TIME_MODE_100 = 2,
			TIME_MODE_60 = 3,
			TIME_MODE_50 = 4,
			TIME_MODE_48 = 5,
			TIME_MODE_30 = 6,
			TIME_MODE_30_DROP = 7,
			TIME_MODE_NTSC_DROP_FRAME = 8,
			TIME_MODE_NTSC_FULL_FRAME = 9,
			TIME_MODE_PAL = 10,
			TIME_MODE_CINEMA = 11,
			TIME_MODE_1000 = 12,
			TIME_MODE_CINEMA_ND = 13,
			TIME_MODE_CUSTOM = 14,
			TIME_MODE_TIME_MODE_COUNT = 15
		};
		enum UpAxis {
			UP_VECTOR_AXIS_X = 1,
			UP_VECTOR_AXIS_Y = 2,
			UP_VECTOR_AXIS_Z = 3
		};
		enum FrontAxis {
			FRONT_PARITY_EVEN = 1,
			FRONT_PARITY_ODD = 2,
		};

		enum CoordAxis {
			COORD_RIGHT = 0,
			COORD_LEFT = 1
		};
	};

	static void set_texture_mapping_mode(aiTextureMapMode *map_mode, Ref<Texture> texture) {
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

	/** find the texture path for the supplied fbx path inside godot
     * very simple lookup for subfolders etc for a texture which may or may not be in a directory
     */
	static void find_texture_path(const String &r_p_path, String &r_path, bool &r_found) {

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
				find_texture_path(r_p_path, dir, r_path, r_found, "." + exts[i]);
			}
		}
	}

	/** find the texture path for the supplied fbx path inside godot
     * very simple lookup for subfolders etc for a texture which may or may not be in a directory
     */
	static void find_texture_path(const String &p_path, _Directory &dir, String &path, bool &found, String extension) {
		String name = path.get_basename() + extension;
		if (dir.file_exists(name)) {
			found = true;
			path = name;
			return;
		}
		String name_ignore_sub_directory = p_path.get_base_dir().plus_file(path.get_file().get_basename()) + extension;
		if (dir.file_exists(name_ignore_sub_directory)) {
			found = true;
			path = name_ignore_sub_directory;
			return;
		}

		String name_find_texture_sub_directory = p_path.get_base_dir() + "/textures/" + path.get_file().get_basename() + extension;
		if (dir.file_exists(name_find_texture_sub_directory)) {
			found = true;
			path = name_find_texture_sub_directory;
			return;
		}
		String name_find_texture_upper_sub_directory = p_path.get_base_dir() + "/Textures/" + path.get_file().get_basename() + extension;
		if (dir.file_exists(name_find_texture_upper_sub_directory)) {
			found = true;
			path = name_find_texture_upper_sub_directory;
			return;
		}
		String name_find_texture_outside_sub_directory = p_path.get_base_dir() + "/../textures/" + path.get_file().get_basename() + extension;
		if (dir.file_exists(name_find_texture_outside_sub_directory)) {
			found = true;
			path = name_find_texture_outside_sub_directory;
			return;
		}

		String name_find_upper_texture_outside_sub_directory = p_path.get_base_dir() + "/../Textures/" + path.get_file().get_basename() + extension;
		if (dir.file_exists(name_find_upper_texture_outside_sub_directory)) {
			found = true;
			path = name_find_upper_texture_outside_sub_directory;
			return;
		}
	}

	/** Get assimp string
    * automatically filters the string data
    */
	static String get_assimp_string(const aiString &p_string) {
		//convert an assimp String to a Godot String
		String name;
		name.parse_utf8(p_string.C_Str() /*,p_string.length*/);
		if (name.find(":") != -1) {
			String replaced_name = name.split(":")[1];
			print_verbose("Replacing " + name + " containing : with " + replaced_name);
			name = replaced_name;
		}

		return name;
	}

	static String get_anim_string_from_assimp(const aiString &p_string) {

		String name;
		name.parse_utf8(p_string.C_Str() /*,p_string.length*/);
		if (name.find(":") != -1) {
			String replaced_name = name.split(":")[1];
			print_verbose("Replacing " + name + " containing : with " + replaced_name);
			name = replaced_name;
		}
		return name;
	}

	/**
     * No filter logic get_raw_string_from_assimp
     * This just convers the aiString to a parsed utf8 string
     * Without removing special chars etc
     */
	static String get_raw_string_from_assimp(const aiString &p_string) {
		String name;
		name.parse_utf8(p_string.C_Str() /*,p_string.length*/);
		return name;
	}

	static Ref<Animation> import_animation(const String &p_path, uint32_t p_flags, int p_bake_fps) {
		return Ref<Animation>();
	}

	/**
     * Converts aiMatrix4x4 to godot Transform 
    */
	static const Transform assimp_matrix_transform(const aiMatrix4x4 p_matrix) {
		aiMatrix4x4 matrix = p_matrix;
		Transform xform;
		xform.set(matrix.a1, matrix.a2, matrix.a3, matrix.b1, matrix.b2, matrix.b3, matrix.c1, matrix.c2, matrix.c3, matrix.a4, matrix.b4, matrix.c4);
		return xform;
	}

	/** Get fbx fps for time mode meta data - todo: migrate into assimp
     */
	static float get_fbx_fps(int32_t time_mode, const aiScene *p_scene) {
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
				int32_t frame_rate = -1;
				p_scene->mMetaData->Get("FrameRate", frame_rate);
				return frame_rate;
		}
		return 0;
	}

	/**
     * Get global transform for the current node - so we can use world space rather than
     * local space coordinates
     * useful if you need global - although recommend using local wherever possible over global
     * as you could break fbx scaling :)
     */
	static Transform _get_global_assimp_node_transform(const aiNode *p_current_node) {
		aiNode const *current_node = p_current_node;
		Transform xform;
		while (current_node != NULL) {
			xform = assimp_matrix_transform(current_node->mTransformation) * xform;
			current_node = current_node->mParent;
		}
		return xform;
	}
};

#endif // IMPORT_UTILS_IMPORTER_ASSIMP_H