/*************************************************************************/
/*  skeleton_ik_object.h                                                 */
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

#ifndef SKELETON_IK_OBJECT_H
#define SKELETON_IK_OBJECT_H

#ifndef _3D_DISABLED

#include "core/math/transform.h"
#include "scene/3d/skeleton.h"

class SkeletonIKObject : public Node {
	GDCLASS(SkeletonIKObject, Node);

protected:
	virtual void
	_validate_property(PropertyInfo &property) const {}

	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_root_bone", "root_bone"), &SkeletonIKObject::set_root_bone);
		ClassDB::bind_method(D_METHOD("get_root_bone"), &SkeletonIKObject::get_root_bone);

		ClassDB::bind_method(D_METHOD("set_tip_bone", "tip_bone"), &SkeletonIKObject::set_tip_bone);
		ClassDB::bind_method(D_METHOD("get_tip_bone"), &SkeletonIKObject::get_tip_bone);

		ClassDB::bind_method(D_METHOD("set_interpolation", "interpolation"), &SkeletonIKObject::set_interpolation);
		ClassDB::bind_method(D_METHOD("get_interpolation"), &SkeletonIKObject::get_interpolation);

		ClassDB::bind_method(D_METHOD("set_target_transform", "target"), &SkeletonIKObject::set_target_transform);
		ClassDB::bind_method(D_METHOD("get_target_transform"), &SkeletonIKObject::get_target_transform);

		ClassDB::bind_method(D_METHOD("set_target_node", "node"), &SkeletonIKObject::set_target_node);
		ClassDB::bind_method(D_METHOD("get_target_node"), &SkeletonIKObject::get_target_node);

		ClassDB::bind_method(D_METHOD("set_override_tip_basis", "override"), &SkeletonIKObject::set_override_tip_basis);
		ClassDB::bind_method(D_METHOD("is_override_tip_basis"), &SkeletonIKObject::is_override_tip_basis);

		ClassDB::bind_method(D_METHOD("set_use_magnet", "use"), &SkeletonIKObject::set_use_magnet);
		ClassDB::bind_method(D_METHOD("is_using_magnet"), &SkeletonIKObject::is_using_magnet);

		ClassDB::bind_method(D_METHOD("set_magnet_position", "local_position"), &SkeletonIKObject::set_magnet_position);
		ClassDB::bind_method(D_METHOD("get_magnet_position"), &SkeletonIKObject::get_magnet_position);

		ClassDB::bind_method(D_METHOD("get_parent_skeleton"), &SkeletonIKObject::get_parent_skeleton);
		ClassDB::bind_method(D_METHOD("is_running"), &SkeletonIKObject::is_running);

		ClassDB::bind_method(D_METHOD("set_min_distance", "min_distance"), &SkeletonIKObject::set_min_distance);
		ClassDB::bind_method(D_METHOD("get_min_distance"), &SkeletonIKObject::get_min_distance);

		ClassDB::bind_method(D_METHOD("set_max_iterations", "iterations"), &SkeletonIKObject::set_max_iterations);
		ClassDB::bind_method(D_METHOD("get_max_iterations"), &SkeletonIKObject::get_max_iterations);

		ClassDB::bind_method(D_METHOD("start", "one_time"), &SkeletonIKObject::start, DEFVAL(false));
		ClassDB::bind_method(D_METHOD("stop"), &SkeletonIKObject::stop);

		ADD_PROPERTY(PropertyInfo(Variant::STRING, "root_bone"), "set_root_bone", "get_root_bone");
		ADD_PROPERTY(PropertyInfo(Variant::STRING, "tip_bone"), "set_tip_bone", "get_tip_bone");
		ADD_PROPERTY(PropertyInfo(Variant::REAL, "interpolation", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_interpolation", "get_interpolation");
		ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM, "target"), "set_target_transform", "get_target_transform");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "override_tip_basis"), "set_override_tip_basis", "is_override_tip_basis");
		ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_magnet"), "set_use_magnet", "is_using_magnet");
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "magnet"), "set_magnet_position", "get_magnet_position");
		ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_node"), "set_target_node", "get_target_node");
		ADD_PROPERTY(PropertyInfo(Variant::REAL, "min_distance"), "set_min_distance", "get_min_distance");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "max_iterations"), "set_max_iterations", "get_max_iterations");
	}
	virtual void _notification(int p_what) {}

public:
	SkeletonIKObject() { }
	virtual ~SkeletonIKObject() {}

	virtual void set_root_bone(const StringName &p_root_bone) = 0;
	virtual StringName get_root_bone() const = 0;

	virtual void set_tip_bone(const StringName &p_tip_bone) = 0;
	virtual StringName get_tip_bone() const = 0;

	virtual void set_interpolation(real_t p_interpolation) = 0;
	virtual real_t get_interpolation() const = 0;

	virtual void set_target_transform(const Transform &p_target) = 0;
	virtual const Transform &get_target_transform() const = 0;

	virtual void set_target_node(const NodePath &p_node) = 0;
	virtual NodePath get_target_node() = 0;

	virtual void set_override_tip_basis(bool p_override) = 0;
	virtual bool is_override_tip_basis() const = 0;

	virtual void set_use_magnet(bool p_use) = 0;
	virtual bool is_using_magnet() const = 0;

	virtual void set_magnet_position(const Vector3 &p_local_position) = 0;
	virtual const Vector3 &get_magnet_position() const = 0;

	virtual void set_min_distance(real_t p_min_distance) = 0;
	virtual real_t get_min_distance() const = 0;

	virtual void set_max_iterations(int p_iterations) = 0;
	virtual int get_max_iterations() const = 0;

	virtual Skeleton *get_parent_skeleton() const = 0;

	virtual bool is_running() = 0;

	virtual void start(bool p_one_time = false) = 0;
	virtual void stop() = 0;
};

#endif // _3D_DISABLED

#endif // SKELETON_IK_OBJECT_H
