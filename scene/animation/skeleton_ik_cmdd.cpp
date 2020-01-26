/*************************************************************************/
/*  skeleton_ik_cmdd.cpp                                                 */
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

#ifndef _3D_DISABLED

#include "skeleton_ik_cmdd.h"
#include "scene/animation/skeleton_ik_cmdd.h"

void CMDDInverseKinematic::QCPSolver(
		Ref<Chain> p_chain,
		float p_dampening,
		bool p_inverse_weighting,
		int p_stabilization_passes,
		int p_iteration,
		float p_total_iterations) {
	for (int32_t tip_i = 0; tip_i < p_chain->targets.size(); tip_i++) {
		Ref<ChainItem> start_from = p_chain->targets[tip_i]->chain_item;
		Ref<ChainItem> stop_after = &p_chain->chain_root;

		Ref<ChainItem> current_bone = start_from;
		//if the tip is pinned, it should have already been oriented before this function was called.
		while (current_bone.is_valid() && current_bone != stop_after) {
			if (!current_bone->ik_orientation_lock) {
				update_optimal_rotation_to_target_descendants(p_chain, current_bone, p_dampening, false,
						p_stabilization_passes, p_iteration, p_total_iterations);
			}
			current_bone = current_bone->parent_item;
		}
	}
}

Ref<CMDDInverseKinematic::ChainItem> CMDDInverseKinematic::ChainItem::find_child(const BoneId p_bone_id) {
	for (int i = children.size() - 1; 0 <= i; --i) {
		if (p_bone_id == children[i]->bone) {
			return children.write[i];
		}
	}
	return NULL;
}

Ref<CMDDInverseKinematic::ChainItem> CMDDInverseKinematic::ChainItem::add_child(const BoneId p_bone_id) {
	const int infant_child_id = children.size();
	children.resize(infant_child_id + 1);
	if (children.write[infant_child_id].is_null()) {
		children.write[infant_child_id].instance();
	}
	children.write[infant_child_id]->bone = p_bone_id;
	children.write[infant_child_id]->parent_item = Ref<ChainItem>(this);
	return children.write[infant_child_id];
}

void CMDDInverseKinematic::ChainItem::update_cos_dampening() {
	real_t predampening = 1.0f - get_stiffness();
	real_t default_dampening = parent_armature->dampening;
	real_t dampening = parent_item == NULL ? Math_PI : predampening * default_dampening;
	cos_half_dampen = Math::cos(dampening / 2.0f);
	Ref<IKConstraintKusudama> k = constraint;
	if (k.is_valid() && k->get_pain() != 0.0f) {
		springy = true;
		populate_return_dampening_iteration_array(k);
	} else {
		springy = false;
	}
}

real_t CMDDInverseKinematic::ChainItem::get_stiffness() const {
	return stiffness_scalar;
}

void CMDDInverseKinematic::ChainItem::set_axes_to_returned(IKAxes p_global, IKAxes p_to_set, IKAxes p_limiting_axes,
		real_t p_cos_half_angle_dampen, real_t p_angle_dampen) {
	if (constraint.is_valid()) {
		constraint->set_axes_to_returnful(p_global, p_to_set, p_limiting_axes, p_cos_half_angle_dampen,
				p_angle_dampen);
	}
}

void CMDDInverseKinematic::ChainItem::set_axes_to_be_snapped(IKAxes p_to_set, IKAxes p_limiting_axes,
		real_t p_cos_half_angle_dampen) {
	if (constraint.is_valid()) {
		constraint->set_axes_to_snapped(p_to_set, p_limiting_axes, p_cos_half_angle_dampen);
	}
}

void CMDDInverseKinematic::ChainItem::set_stiffness(real_t p_stiffness) {
	stiffness_scalar = p_stiffness;
	if (parent_item.is_valid()) {
		parent_item->update_cos_dampening();
	}
}

void IKConstraintKusudama::optimize_limiting_axes() {
	const IKAxes original_limiting_axes = limiting_axes;

	Vector<Vector3> directions;
	if (direction_limits.size() == 1) {
		Ref<IKDirectionLimit> direction_limit = direction_limits[0];
		directions.push_back(directions.write[0] + direction_limit->get_control_point().normalized());
	} else {
		for (int limit_i = 0; limit_i < direction_limits.size() - 1; limit_i++) {
			Ref<IKDirectionLimit> direction_limit_0 = direction_limits[limit_i];
			Vector3 this_c = direction_limit_0->get_control_point().normalized();
			Ref<IKDirectionLimit> direction_limit_1 = direction_limits[limit_i + 1];
			Vector3 next_c = direction_limit_1->get_control_point().normalized();
			Quat this_to_next = Quat(this_c, next_c);
			Vector3 axis;
			real_t angle;
			Basis(this_to_next).get_axis_angle(axis, angle);
			Quat half_this_to_next = Quat(axis, angle / 2.0f);

			Vector3 half_angle = Basis(half_this_to_next).rotated(this_c).get_euler();
			half_angle.normalize();
			half_angle *= this_to_next.get_euler();
			directions.push_back(half_angle);
		}
	}

	Vector3 newY;
	for (int32_t direction_i = 0; direction_i <
								  directions.size();
			direction_i++) {
		newY += directions[direction_i];
	}

	newY /= directions.size();
	if (newY.length() != 0 && !Math::is_nan(newY.y)) {
		newY.normalize();
	} else {
		newY = Vector3(0.0, 1.0f, 0.0);
	}

	Ray newYRay = Ray(Vector3(0.0, 0.0, 0.0), newY);

	//TODO
	//Quat old_y_to_new_y = Quat(limiting_axes.y_().heading(), original_limiting_axes.getGlobalOf(newYRay).heading());
	Transform xform;
	xform.basis = original_limiting_axes.get_basis();
	Quat old_y_to_new_y = limiting_axes.get_basis() * xform.get_basis();
	limiting_axes.basis.rotate(old_y_to_new_y);

	for (int32_t direction_limit_i = 0; direction_limit_i < direction_limits.size(); direction_limit_i++) {
		Ref<IKDirectionLimit> direction_limit = direction_limits[direction_limit_i];
		// TODO
		// original_limiting_axes.setToGlobalOf(direction_limit->get_control_point(), direction_limit->get_control_point().normalized());
		// limiting_axes.setToLocalOf(direction_limit->get_control_point(), direction_limit->get_control_point().normalized());
		direction_limit->get_control_point().normalize();
	}
	update_tangent_radii();
}

void IKConstraintKusudama::set_axial_limits(float p_min_angle, float p_in_range) {
	axial_limit->set_min_twist_angle(p_min_angle);
	axial_limit->set_range(p_in_range);
	constraint_update_notification();
}

void IKConstraintKusudama::add_direction_limit_at_index(int p_insert_at, Vector3 p_new_point, float p_radius) {
	Ref<IKDirectionLimit> new_direction_limit = create_direction_limit_for_index(p_insert_at, p_new_point, p_radius);
	ERR_FAIL_COND(new_direction_limit.is_null());
	if (p_insert_at == -1) {
		direction_limits.push_back(new_direction_limit);
	} else {
		direction_limits.insert(p_insert_at, new_direction_limit);
	}
	update_tangent_radii();
	update_rotational_freedom();
}

Ref<IKDirectionLimit>
IKConstraintKusudama::create_direction_limit_for_index(int p_insert_at, Vector3 p_new_point, float p_radius) {
	ERR_FAIL_INDEX_V(p_insert_at, direction_limits.size(), NULL);
	Ref<IKDirectionLimit> direction_limit;
	direction_limit.instance();
	direction_limit->initialize(p_new_point, p_radius, this);
	direction_limits.write[p_insert_at] = direction_limit;
	return direction_limit;
}

/// Build a chain that starts from the root to tip
bool CMDDInverseKinematic::build_chain(Task *p_task, bool p_force_simple_chain) {
	ERR_FAIL_COND_V(-1 == p_task->root_bone, false);

	Ref<Chain> chain = p_task->chain;
	chain->targets.resize(p_task->end_effectors.size());
	chain->chain_root.bone = p_task->root_bone;
	chain->chain_root.axes->local_transform = p_task->skeleton->get_bone_rest(chain->chain_root.bone) * p_task->skeleton->get_bone_pose(chain->chain_root.bone);
	chain->chain_root.pb = p_task->skeleton->get_physical_bone(chain->chain_root.bone);
	chain->middle_chain_item = Ref<ChainItem>();

	// Holds all IDs that are composing a single chain in reverse order
	Vector<BoneId> chain_ids;
	// This is used to know the chain size
	int sub_chain_size = 0;
	// Resize only one time in order to fit all joints for performance reason
	chain_ids.resize(p_task->skeleton->get_bone_count());

	for (int x = p_task->end_effectors.size() - 1; 0 <= x; --x) {

		const Ref<EndEffector> ee(p_task->end_effectors[x]);
		ERR_FAIL_COND_V(p_task->root_bone >= ee->effector_bone, false);
		ERR_FAIL_INDEX_V(ee->effector_bone, p_task->skeleton->get_bone_count(), false);

		sub_chain_size = 0;
		// Picks all IDs that composing a single chain in reverse order (except the root)
		BoneId chain_sub_tip(ee->effector_bone);
		while (chain_sub_tip > p_task->root_bone) {

			chain_ids.write[sub_chain_size++] = chain_sub_tip;
			chain_sub_tip = p_task->skeleton->get_bone_parent(chain_sub_tip);
		}

		BoneId middle_chain_item_id = (((float)sub_chain_size) * 0.5);

		// Build chain by reading chain ids in reverse order
		// For each chain item id will be created a ChainItem if doesn't exists
		Ref<ChainItem> sub_chain(&chain->chain_root);
		for (int i = sub_chain_size - 1; 0 <= i; --i) {

			Ref<ChainItem> child_ci(sub_chain->find_child(chain_ids[i]));
			if (child_ci.is_null()) {

				child_ci = sub_chain->add_child(chain_ids[i]);

				child_ci->pb = p_task->skeleton->get_physical_bone(child_ci->bone);
				if (child_ci->parent_item.is_valid()) {
					child_ci->axes->parent = child_ci->parent_item->axes;
					child_ci->parent_item->children.push_back(child_ci->axes);
				}
				child_ci->axes->local_transform = p_task->skeleton->get_bone_rest(child_ci->bone) * p_task->skeleton->get_bone_pose(child_ci->bone);

				if (child_ci->parent_item.is_valid()) {
					child_ci->length = (child_ci->axes->local_transform.origin - child_ci->parent_item->axes->local_transform.origin).length();
				}
			}

			sub_chain = child_ci;

			if (middle_chain_item_id == i) {
				chain->middle_chain_item = child_ci;
			}
		}

		if (!middle_chain_item_id)
			chain->middle_chain_item = Ref<ChainItem>();

		if (chain->targets.write[x].is_null()) {
			chain->targets.write[x].instance();
		}

		// Initialize current tip
		chain->targets.write[x]->chain_item = sub_chain;
		chain->targets.write[x]->end_effector = ee;

		print_verbose("IK build chain bone " + p_task->skeleton->get_bone_name(chain->targets.write[x]->chain_item->bone));
		print_verbose("IK build chain bone local location " + chain->targets.write[x]->chain_item->axes->local_transform.origin);
		print_verbose("IK build chain bone global location " + p_task->skeleton->get_bone_global_pose(chain->targets.write[x]->chain_item->bone).origin);

		// Allow multieffector
		// if (p_force_simple_chain) {
		// 	// NOTE:
		// 	//	This is an "hack" that force to create only one tip per chain since the solver of multi tip (end effector)
		// 	//	is not yet created.
		// 	//	Remove this code when this is done
		// 	break;
		// }
	}
	if (chain->constraints.is_valid() && sub_chain_size) {
		chain->constraints->set_constraint_count(sub_chain_size);
		for (int32_t count_i = 0; count_i < sub_chain_size; count_i++) {
			StringName bone_name = p_task->skeleton->get_bone_name(chain_ids[count_i]);
			chain->constraints->set_chain_item(count_i, bone_name);
			Ref<IKConstraintKusudama> constraint = chain->constraints->get_constraint(count_i);
			chain->constraints->set_constraint(count_i, constraint);
		}
		chain->constraints->_change_notify();
	}

	chain->create_headings_arrays();

	return true;
}

void CMDDInverseKinematic::update_chain(const Skeleton *p_sk, Ref<ChainItem> p_chain_item) {

	if (p_chain_item.is_null())
		return;
	p_chain_item->axes->local_transform =
			p_sk->get_bone_rest(p_chain_item->bone) * p_sk->get_bone_pose(p_chain_item->bone);

	for (int i = p_chain_item->children.size() - 1; 0 <= i; --i) {
		update_chain(p_sk, p_chain_item->children.write[i]);
	}
}

void CMDDInverseKinematic::solve_simple(Task *p_task, bool p_solve_magnet) {
	QCPSolver(
			p_task->chain,
			p_task->dampening,
			false,
			p_task->iterations,
			p_task->stabilizing_passes,
			p_task->max_iterations);
}

CMDDInverseKinematic::Task *CMDDInverseKinematic::create_simple_task(Skeleton *p_sk, BoneId root_bone, BoneId tip_bone,
		const Transform &goal_transform,
		real_t p_dampening, int p_stabilizing_passes,
		Ref<SkeletonIKConstraints> p_constraints) {

	Ref<CMDDInverseKinematic::EndEffector> ee;
	ee.instance();
	ee->effector_bone = tip_bone;

	Task *task(memnew(Task));
	task->skeleton = p_sk;
	task->root_bone = root_bone;
	task->end_effectors.push_back(ee);
	task->goal_global_transform = goal_transform;
	task->dampening = p_dampening;
	task->stabilizing_passes = p_stabilizing_passes;
	task->chain->constraints = p_constraints;
	PoolStringArray bone_names;
	for (int32_t bone_i = 0; bone_i < p_sk->get_bone_count(); bone_i++) {
		bone_names.push_back(p_sk->get_bone_name(bone_i));
	}
	task->chain->constraints->set_bone_names(bone_names);
	if (!build_chain(task)) {
		free_task(task);
		return NULL;
	}

	return task;
}

void CMDDInverseKinematic::free_task(Task *p_task) {
	if (p_task)
		memdelete(p_task);
}

void CMDDInverseKinematic::set_goal(Task *p_task, const Transform &p_goal) {
	p_task->goal_global_transform = p_goal;
}

void CMDDInverseKinematic::make_goal(Task *p_task, const Transform &p_inverse_transf, real_t blending_delta) {

	if (blending_delta >= 0.99f) {
		// Update the end_effector (local transform) without blending
		p_task->end_effectors.write[0]->goal_transform = p_inverse_transf * p_task->goal_global_transform;
	} else {

		// End effector in local transform
		const Transform end_effector_pose(
				p_task->skeleton->get_bone_rest(p_task->end_effectors.write[0]->effector_bone) * p_task->skeleton->get_bone_pose(p_task->end_effectors.write[0]->effector_bone));
		print_verbose("IK make goal bone " + p_task->skeleton->get_bone_name(p_task->end_effectors.write[0]->effector_bone));
		print_verbose("IK make goal bone local location " + end_effector_pose.origin);
		print_verbose("IK make goal bone global location " + p_task->skeleton->get_bone_global_pose(p_task->end_effectors.write[0]->effector_bone).origin);
		// Update the end_effector (local transform) by blending with current pose
		p_task->end_effectors.write[0]
				->goal_transform = end_effector_pose.interpolate_with(
				p_inverse_transf * p_task->goal_global_transform, blending_delta);
	}
}

void CMDDInverseKinematic::solve(Task *p_task, real_t blending_delta, bool override_tip_basis, bool p_use_magnet,
		const Vector3 &p_magnet_position) {

	if (blending_delta <= 0.01f) {
		return; // Skip solving
	}

	make_goal(p_task, p_task->skeleton->get_global_transform().affine_inverse().scaled(p_task->skeleton->get_global_transform().get_basis().get_scale()), blending_delta);

	update_chain(p_task->skeleton, &p_task->chain->chain_root);

	print_verbose("IK solve bone " + p_task->skeleton->get_bone_name(p_task->end_effectors[0]->effector_bone));
	print_verbose("IK solve bone goal local location " + p_task->end_effectors[0]->goal_transform.origin);
	Ref<ChainItem> effector_chain_item = p_task->chain->chain_root.find_child(p_task->end_effectors[0]->effector_bone);
	if (effector_chain_item.is_valid()) {
		print_verbose("IK solve bone local location " + effector_chain_item->axes->local_transform.origin);
		print_verbose("IK solve bone global location " + p_task->skeleton->get_bone_global_pose(effector_chain_item->bone).origin);
	}

	// if (p_use_magnet && p_task->chain->middle_chain_item.is_valid()) {
	// 	p_task->chain->magnet_position = p_task->chain->middle_chain_item->get_global_transform().origin.linear_interpolate(
	// 			p_magnet_position, blending_delta);
	// 	solve_simple(p_task, true);
	// }
	solve_simple(p_task, false);

	// Assign new bone position.
	Ref<ChainItem> ci(&p_task->chain->chain_root);
	while (ci.is_valid()) {
		Transform new_bone_pose(ci->get_global_transform());
		new_bone_pose.origin = ci->axes->local_transform.origin;

		if (!ci->children.empty()) {

			/// Rotate basis
			const Vector3 initial_ori(
					(ci->children[0]->get_global_transform().origin - ci->get_global_transform().origin).normalized());
			const Vector3 rot_axis(initial_ori.cross(ci->axes->local_transform.get_basis().get_rotation_euler()).normalized());

			if (rot_axis[0] != 0 && rot_axis[1] != 0 && rot_axis[2] != 0) {
				const real_t rot_angle(Math::acos(CLAMP(initial_ori.dot(ci->axes->local_transform.get_basis().get_rotation_euler()), -1, 1)));
				new_bone_pose.basis.rotate(rot_axis, rot_angle);
			}
		} else {
			// Set target orientation to tip
			if (override_tip_basis)
				new_bone_pose.basis = p_task->chain->targets[0]->end_effector->goal_transform.basis;
			else
				new_bone_pose.basis =
						new_bone_pose.basis * p_task->chain->targets[0]->end_effector->goal_transform.basis;
		}

		p_task->skeleton->set_bone_global_pose_override(ci->bone, new_bone_pose, 1.0);

		if (!ci->children.empty())
			ci = ci->children.write[0];
		else
			ci = Ref<ChainItem>();
	}
}

void CMDDInverseKinematic::set_default_dampening(Ref<CMDDInverseKinematic::Chain> r_chain, real_t p_damp) {
	r_chain->dampening =
			MIN(Math_PI * 3.0f, MAX(Math::absf(std::numeric_limits<real_t>::epsilon()), Math::absf(p_damp)));
	update_armature_segments(r_chain);
}

void CMDDInverseKinematic::update_armature_segments(Ref<CMDDInverseKinematic::Chain> r_chain) {
	r_chain->bone_segment_map.clear();
	recursively_update_bone_segment_map_from(r_chain, &r_chain->chain_root);
}

void CMDDInverseKinematic::update_optimal_rotation_to_target_descendants(Ref<CMDDInverseKinematic::ChainItem> p_chain_item,
		real_t p_dampening, bool p_is_translate,
		PoolVector3Array p_localized_tip_headings,
		PoolVector3Array p_localized_target_headings,
		PoolRealArray p_weights,
		QCP p_qcp_orientation_aligner, int p_iteration,
		real_t p_total_iterations) {

	p_qcp_orientation_aligner.set_max_iterations(10);
	IKQuat qcp_rot = p_qcp_orientation_aligner.weighted_superpose(p_localized_tip_headings, p_localized_target_headings,
			p_weights, p_is_translate);

	Vector3 translate_by = p_qcp_orientation_aligner.get_translation();
	real_t bone_damp = p_chain_item->cos_half_dampen;

	if (p_dampening != -1) {
		bone_damp = p_dampening;
		qcp_rot.clamp_to_angle(bone_damp);
	} else {
		qcp_rot.clamp_to_quadrance_angle(bone_damp);
	}
	p_chain_item->axes->local_transform.origin *= translate_by;
	p_chain_item->axes->local_transform.basis *= Basis(qcp_rot);
	IKAxes xform;
	xform.basis = p_chain_item->axes->local_transform.get_basis();
	if (p_chain_item->constraint.is_null()) {
		return;
	}
	p_chain_item->set_axes_to_be_snapped(xform, p_chain_item->constraint->get_limiting_axes(), bone_damp);
	xform.origin = p_chain_item->axes->local_transform.origin;
	p_chain_item->constraint->set_limiting_axes(p_chain_item->constraint->get_limiting_axes().translated(translate_by));
}

void CMDDInverseKinematic::recursively_update_bone_segment_map_from(Ref<CMDDInverseKinematic::Chain> r_chain,
		CMDDInverseKinematic::ChainItem *p_start_from) {
	for (int32_t child_i = 0; child_i < p_start_from->children.size(); child_i++) {
		r_chain->bone_segment_map.insert(p_start_from->children[child_i]->bone, p_start_from);
	}
}

void CMDDInverseKinematic::update_optimal_rotation_to_target_descendants(Ref<CMDDInverseKinematic::Chain> r_chain,
		Ref<CMDDInverseKinematic::ChainItem> p_for_bone,
		float p_dampening, bool p_translate,
		int p_stabilization_passes, int p_iteration,
		int p_total_iterations) {
	if (p_for_bone.is_null()) {
		return;
	}

	Quat best_orientation = p_for_bone->axes->local_transform.get_basis().get_rotation_quat();
	float new_dampening = -1;
	if (p_for_bone->parent_item == NULL)
		p_stabilization_passes = 0;
	if (p_translate == true) {
		new_dampening = Math_PI;
	}
	IKAxes bone_xform;
	Quat quat = p_for_bone->axes->local_transform.basis.get_rotation_quat();
	bone_xform.basis = Basis(quat);
	update_target_headings(r_chain, r_chain->localized_target_headings, r_chain->weights, bone_xform);
	update_effector_headings(r_chain, r_chain->localized_effector_headings, bone_xform);

	float best_rmsd = 0.0f;
	QCP qcp_convergence_check = QCP(FLT_EPSILON, FLT_EPSILON);
	float new_rmsd = 999999.0f;

	if (p_stabilization_passes > 0) {
		best_rmsd = get_manual_msd(r_chain->localized_effector_headings, r_chain->localized_target_headings,
				r_chain->weights);
	}

	for (int stabilization_i = 0; stabilization_i < p_stabilization_passes + 1; stabilization_i++) {
		update_optimal_rotation_to_target_descendants(
				p_for_bone, new_dampening,
				p_translate,
				r_chain->localized_effector_headings,
				r_chain->localized_target_headings,
				r_chain->weights,
				qcp_convergence_check,
				p_iteration,
				p_total_iterations);

		if (p_stabilization_passes > 0) {
			update_effector_headings(r_chain, r_chain->localized_effector_headings, bone_xform);
			new_rmsd = get_manual_msd(r_chain->localized_effector_headings, r_chain->localized_target_headings,
					r_chain->weights);

			if (best_rmsd >= new_rmsd) {
				if (p_for_bone->springy) {
					if (p_dampening != -1 || p_total_iterations != r_chain->get_default_iterations()) {
						real_t returnfullness = p_for_bone->constraint->get_pain();
						real_t dampened_angle = p_for_bone->get_stiffness() * p_dampening * returnfullness;
						real_t total_iterations_sq = p_total_iterations * p_total_iterations;
						real_t scaled_dampened_angle = dampened_angle *
													   ((total_iterations_sq - (p_iteration * p_iteration)) /
															   total_iterations_sq);
						real_t cos_half_angle = Math::cos(0.5f * scaled_dampened_angle);
						p_for_bone->set_axes_to_returned(p_for_bone->get_global_transform(), bone_xform,
								p_for_bone->constraint->get_limiting_axes(), cos_half_angle,
								scaled_dampened_angle);
					} else {
						p_for_bone->set_axes_to_returned(p_for_bone->get_global_transform(), bone_xform,
								p_for_bone->constraint->get_limiting_axes(),
								p_for_bone->cos_half_returnful_dampened[p_iteration],
								p_for_bone->half_returnful_dampened[p_iteration]);
					}
					update_effector_headings(r_chain, r_chain->localized_effector_headings, bone_xform);
					new_rmsd = get_manual_msd(r_chain->localized_effector_headings, r_chain->localized_target_headings,
							r_chain->weights);
				}
				best_orientation = bone_xform.basis.get_rotation_quat();
				best_rmsd = new_rmsd;
				break;
			}
		} else {
			break;
		}
	}
	if (p_stabilization_passes > 0) {
		bone_xform.basis = Basis(best_orientation);
		p_for_bone->axes->local_transform.origin = bone_xform.origin;
		p_for_bone->axes->local_transform.basis = bone_xform.basis;
	}
}

real_t CMDDInverseKinematic::get_manual_msd(PoolVector3Array &r_localized_effector_headings,
		PoolVector3Array &r_localized_target_headings,
		const PoolRealArray &p_weights) {
	real_t manual_rmsd = 0.0f;
	real_t wsum = 0.0f;
	for (int i = 0; i < r_localized_target_headings.size(); i++) {
		real_t xd = r_localized_target_headings[i].x - r_localized_effector_headings[i].x;
		real_t yd = r_localized_target_headings[i].y - r_localized_effector_headings[i].y;
		real_t zd = r_localized_target_headings[i].z - r_localized_effector_headings[i].z;
		real_t mag_sq = p_weights[i] * (xd * xd + yd * yd + zd * zd);
		manual_rmsd += mag_sq;
		wsum += p_weights[i];
	}
	manual_rmsd /= wsum;
	return manual_rmsd;
}

void CMDDInverseKinematic::update_target_headings(Ref<Chain> r_chain, PoolVector3Array &r_localized_target_headings,
		PoolRealArray p_weights, Transform p_bone_xform) {

	int hdx = 0;
	for (int target_i = 0; target_i < r_chain->targets.size(); target_i++) {
		Ref<ChainItem> sb = r_chain->targets[target_i]->chain_item;
		if (sb->constraint.is_null()) {
			continue;
		}
		IKAxes targetAxes = sb->constraint->get_limiting_axes();
		Vector3 origin = sb->axes->local_transform.origin;
		r_localized_target_headings[hdx] = targetAxes.origin - origin;
		uint8_t modeCode = r_chain->targets[target_i]->get_mode_code();
		hdx++;
		if ((modeCode & ChainTarget::XDir) != 0) {
			Ray xTarget;
			xTarget.normal = Vector3(1, 0, 0);
			xTarget.position = xTarget.normal * targetAxes.basis.get_axis(x_axis);
			r_localized_target_headings[hdx] = xTarget.position - origin;
			// xTarget.position = xTarget.setToInvertedTip(r_localized_target_headings[hdx + 1]) - origin;
			hdx += 2;
		}
		if ((modeCode & ChainTarget::YDir) != 0) {
			Ray yTarget;
			yTarget.normal = Vector3(0, 1, 0);
			yTarget.position = yTarget.normal * targetAxes.basis.get_axis(y_axis);
			r_localized_target_headings[hdx] = yTarget.position - origin;
			// yTarget.position = yTarget.setToInvertedTip(r_localized_target_headings[hdx + 1]) - origin;
			hdx += 2;
		}
		if ((modeCode & ChainTarget::ZDir) != 0) {
			Ray zTarget;
			zTarget.normal = Vector3(0, 0, 1);
			zTarget.position = zTarget.normal * targetAxes.basis.get_axis(z_axis);
			r_localized_target_headings[hdx] = zTarget.position - origin;
			// zTarget.position = zTarget.setToInvertedTip(r_localized_target_headings[hdx + 1]) - origin;
			hdx += 2;
		}
	}
}

void CMDDInverseKinematic::update_effector_headings(Ref<Chain> r_chain, PoolVector3Array &r_localized_effector_headings,
		Transform p_bone_xform) {

	int hdx = 0;
	for (int target_i = 0; target_i < r_chain->targets.size(); target_i++) {
		Ref<ChainItem> sb = r_chain->targets[target_i]->chain_item;
		IKAxes effector = r_chain->targets[target_i]->end_effector->goal_transform;
		// tipAxes.updateGlobal();
		Vector3 origin = sb->axes->local_transform.origin;
		r_localized_effector_headings.write()[hdx] = effector.origin - origin;
		uint8_t modeCode = r_chain->targets[target_i]->get_mode_code();
		hdx++;

		if ((modeCode & ChainTarget::XDir) != 0) {
			Ray xEffector;
			xEffector.normal = Vector3(1, 0, 0);
			xEffector.position = xEffector.normal * effector.basis.get_axis(x_axis);

			r_localized_effector_headings.write()[hdx] = xEffector.position - origin;
			// xTip.setToInvertedTip(r_localized_effector_headings[hdx+1]).sub(origin);
			hdx += 2;
		}
		if ((modeCode & ChainTarget::YDir) != 0) {
			Ray yEffector;

			yEffector.normal = Vector3(0, 1, 0);
			yEffector.position = yEffector.normal * effector.basis.get_axis(y_axis);

			r_localized_effector_headings.write()[hdx] = yEffector.position - origin;
			// yEffector.setToInvertedTip(r_localized_effector_headings[hdx+1]).sub(origin);
			hdx += 2;
		}
		if ((modeCode & ChainTarget::ZDir) != 0) {
			Ray zEffector;

			zEffector.normal = Vector3(0, 0, 1);
			zEffector.position = zEffector.normal * effector.basis.get_axis(z_axis);

			r_localized_effector_headings.write()[hdx] = zEffector.position - origin;
			// zEffector.setToInvertedTip(r_localized_effector_headings[hdx+1]).sub(origin);
			hdx += 2;
		}
	}
}

void SkeletonIKCMDD::_validate_property(PropertyInfo &property) const {

	if (property.name == "root_bone" || property.name == "tip_bone") {

		if (skeleton) {

			String names("--,");
			for (int i = 0; i < skeleton->get_bone_count(); i++) {
				if (i > 0)
					names += ",";
				names += skeleton->get_bone_name(i);
			}

			property.hint = PROPERTY_HINT_ENUM;
			property.hint_string = names;
		} else {

			property.hint = PROPERTY_HINT_NONE;
			property.hint_string = "";
		}
	}
}

void IKTwistLimit::set_min_twist_angle(real_t p_min_axial_angle) {
	min_twist_angle = p_min_axial_angle;
}

real_t IKTwistLimit::get_min_twist_angle() const {
	return min_twist_angle;
}
void IKTwistLimit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_min_angle_degree", "angle"), &IKTwistLimit::set_min_twist_angle_degree);
	ClassDB::bind_method(D_METHOD("get_min_angle_degree"), &IKTwistLimit::get_min_twist_angle_degree);
	ClassDB::bind_method(D_METHOD("set_range_degree", "range"), &IKTwistLimit::set_range_degree);
	ClassDB::bind_method(D_METHOD("get_range_degree"), &IKTwistLimit::get_range_degree);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "min_angle_degree"), "set_min_angle_degree", "get_min_angle_degree");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "range_degree"), "set_range_degree", "get_range_degree");
}

real_t IKTwistLimit::get_range() const {
	return range;
}

void IKTwistLimit::set_range(real_t p_range) {
	range = p_range;
}

void SkeletonIKCMDD::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constraints", "constraints"), &SkeletonIKCMDD::set_constraints);
	ClassDB::bind_method(D_METHOD("get_constraints"), &SkeletonIKCMDD::get_constraints);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "constraints", PROPERTY_HINT_RESOURCE_TYPE, "SkeletonIKConstraints"),
			"set_constraints", "get_constraints");
}

void SkeletonIKCMDD::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			skeleton = Object::cast_to<Skeleton>(get_parent());
			set_process_priority(1);
			reload_chain();
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {

			if (target_node_override)
				reload_goal();

			_solve_chain();

		} break;
		case NOTIFICATION_EXIT_TREE: {
			reload_chain();
		} break;
	}
}

SkeletonIKCMDD::SkeletonIKCMDD() :
		interpolation(1),
		override_effector_basis(true),
		use_magnet(false),
		min_distance(0.01),
		max_iterations(10),
		skeleton(NULL),
		target_node_override(NULL),
		task(NULL) {
	constraints.instance();
}

SkeletonIKCMDD::~SkeletonIKCMDD() {
	CMDDInverseKinematic::free_task(task);
	task = NULL;
}

void SkeletonIKCMDD::set_root_bone(const StringName &p_root_bone) {
	root_bone = p_root_bone;
	reload_chain();
}

StringName SkeletonIKCMDD::get_root_bone() const {
	return root_bone;
}

void SkeletonIKCMDD::set_tip_bone(const StringName &p_tip_bone) {
	effector_bone = p_tip_bone;
	reload_chain();
}

StringName SkeletonIKCMDD::get_tip_bone() const {
	return effector_bone;
}

void SkeletonIKCMDD::set_interpolation(real_t p_interpolation) {
	interpolation = p_interpolation;
}

real_t SkeletonIKCMDD::get_interpolation() const {
	return interpolation;
}

void SkeletonIKCMDD::set_target_transform(const Transform &p_target) {
	target = p_target;
	reload_goal();
}

const Transform &SkeletonIKCMDD::get_target_transform() const {
	return target;
}

void SkeletonIKCMDD::set_target_node(const NodePath &p_node) {
	target_node_path_override = p_node;
	target_node_override = NULL;
	reload_goal();
}

NodePath SkeletonIKCMDD::get_target_node() {
	return target_node_path_override;
}

void SkeletonIKCMDD::set_override_tip_basis(bool p_override) {
	override_effector_basis = p_override;
}

bool SkeletonIKCMDD::is_override_tip_basis() const {
	return override_effector_basis;
}

void SkeletonIKCMDD::set_use_magnet(bool p_use) {
	use_magnet = p_use;
}

bool SkeletonIKCMDD::is_using_magnet() const {
	return use_magnet;
}

void SkeletonIKCMDD::set_magnet_position(const Vector3 &p_local_position) {
	magnet_position = p_local_position;
}

const Vector3 &SkeletonIKCMDD::get_magnet_position() const {
	return magnet_position;
}

void SkeletonIKCMDD::set_min_distance(real_t p_min_distance) {
	min_distance = p_min_distance;
}

void SkeletonIKCMDD::set_max_iterations(int p_iterations) {
	max_iterations = p_iterations;
}

bool SkeletonIKCMDD::is_running() {
	return is_processing_internal();
}

void SkeletonIKCMDD::start(bool p_one_time) {
	if (p_one_time) {
		set_process_internal(false);
		_solve_chain();
	} else {
		set_process_internal(true);
	}
}

void SkeletonIKCMDD::stop() {
	set_process_internal(false);
}

Transform SkeletonIKCMDD::_get_target_transform() {

	if (!target_node_override && !target_node_path_override.is_empty())
		target_node_override = Object::cast_to<Spatial>(get_node(target_node_path_override));

	if (target_node_override)
		return target_node_override->get_global_transform();
	else
		return target;
}

void SkeletonIKCMDD::reload_chain() {

	CMDDInverseKinematic::free_task(task);
	task = NULL;

	if (!skeleton) {
		return;
	}

	task = CMDDInverseKinematic::create_simple_task(skeleton, skeleton->find_bone(root_bone),
			skeleton->find_bone(effector_bone), _get_target_transform(), -1, -1,
			get_constraints());
	if (task) {
		task->max_iterations = max_iterations;
		task->min_distance = min_distance;
	}
}

void SkeletonIKCMDD::reload_goal() {
	if (!task) {
		return;
	}

	CMDDInverseKinematic::set_goal(task, _get_target_transform());
}

void SkeletonIKCMDD::_solve_chain() {
	if (!task) {
		return;
	}
	CMDDInverseKinematic::solve(task, interpolation, override_effector_basis, use_magnet, magnet_position);
}

void SkeletonIKCMDD::set_constraints(const Ref<SkeletonIKConstraints> p_constraints) {
	constraints = p_constraints;
}

Ref<SkeletonIKConstraints> SkeletonIKCMDD::get_constraints() const {
	return constraints;
}

Skeleton *SkeletonIKCMDD::get_parent_skeleton() const {
	return skeleton;
}

#endif // _3D_DISABLED

void IKDirectionLimit::initialize(Vector3 p_location, real_t p_rad, Ref<IKConstraintKusudama> p_attached_to) {
	set_control_point(p_location);
	tangent_circle_center_next_1 = get_orthogonal(p_location);
	tangent_circle_center_next_2 = tangent_circle_center_next_1 * -1.0f;

	radius = MAX(std::numeric_limits<real_t>::min(), p_rad);
	radius_cosine = Math::cos(radius);
	parent_kusudama = p_attached_to;
}

void IKDirectionLimit::update_tangent_handles(Ref<IKDirectionLimit> p_next) {
	control_point.normalize();

	real_t radA = get_radius();
	real_t radB = p_next->get_radius();

	Vector3 A = get_control_point().normalized();
	Vector3 B = get_control_point().normalized();

	Vector3 arcNormal = A.cross(B);
	Quat aToARadian = Quat(A, arcNormal);
	Vector3 aToARadianAxis;
	real_t aToARadianAngle;
	Basis(aToARadian).get_axis_angle(aToARadianAxis, aToARadianAngle);
	Quat bToBRadian = Quat(B, arcNormal);
	Vector3 bToBRadianAxis;
	real_t bToBRadianAngle;
	Basis(bToBRadian).get_axis_angle(bToBRadianAxis, bToBRadianAngle);
	aToARadian = Quat(aToARadianAxis, radA);
	bToBRadian = Quat(bToBRadianAxis, radB);

	/**
     * There are an infinite number of circles co-tangent with A and B, every other
     * one of which had a unique radius.
     *
     * However, we want the radius of our tangent circles to obey the following properties:
     *   1) When the radius of A + B == 0, our tangent circle's radius should = 90.
     *   	In other words, the tangent circle should span a hemisphere.
     *   2) When the radius of A + B == 180, our tangent circle's radius should = 0.
     *   	In other words, when A + B combined are capable of spanning the entire sphere,
     *   	our tangentCircle should be nothing.
     *
     * Another way to think of this is -- whatever the maximum distance can be between the
     * borders of A and B (presuming their centers are free to move about the circle
     * but their radii remain constant), we want our tangentCircle's diameter to be precisely that distance.
     */

	real_t tRadius = ((Math_PI) - (radA + radB)) / 2.0f;

	/**
     * Once we have the desired radius for our tangent circle, we may find the solution for its
     * centers (usually, there are two).
     */

	real_t minorAppoloniusRadiusA = radA + tRadius;
	Vector3 minorAppoloniusAxisA = A.normalized();
	real_t minorAppoloniusRadiusB = radB + tRadius;
	Vector3 minorAppoloniusAxisB = B.normalized();

	//the point on the radius of this cone + half the arcdistance to the circumference of the next cone along the arc path to the next cone
	Vector3 minorAppoloniusP1A = Quat(arcNormal, minorAppoloniusRadiusA).get_euler() * minorAppoloniusAxisA;
	//the point on the radius of this cone + half the arcdistance to the circumference of the next cone, rotated 90 degrees along the axis of this cone
	Vector3 minorAppoloniusP2A = Quat(minorAppoloniusAxisA, Math_PI / 2.0f).get_euler() * minorAppoloniusP1A;
	//the axis of this cone, scaled to minimize its distance to the previous two points.
	Vector3 minorAppoloniusP3A = minorAppoloniusAxisA * Math::cos(minorAppoloniusRadiusA);

	Vector3 minorAppoloniusP1B = Quat(arcNormal, minorAppoloniusRadiusB).get_euler() * minorAppoloniusAxisB;
	Vector3 minorAppoloniusP2B = Quat(minorAppoloniusAxisB, Math_PI / 2.0f).get_euler() * minorAppoloniusP1B;
	Vector3 minorAppoloniusP3B = minorAppoloniusAxisB * Math::cos(minorAppoloniusRadiusB);

	// ray from scaled center of next cone to half way point between the circumference of this cone and the next cone.
	Ray r1B = Ray(minorAppoloniusP1B, minorAppoloniusP3B);
	r1B.elongate(99.0f);
	Ray r2B = Ray(minorAppoloniusP1B, minorAppoloniusP2B);
	r2B.elongate(99.0f);

	Vector3 intersection1;
	Plane plane = Plane(minorAppoloniusP3A, minorAppoloniusP1A, minorAppoloniusP2A);
	plane.intersects_ray(r1B.position, r1B.normal, &intersection1);
	Vector3 intersection2;
	plane.intersects_ray(r2B.position, r2B.normal, &intersection2);

	Ray intersectionRay = Ray(intersection1, intersection2);
	intersectionRay.elongate(99.0f);

	Vector3 sphereIntersect1;
	Vector3 sphereIntersect2;
	Vector3 sphereCenter;

	intersectionRay.intersects_sphere(sphereCenter, 1.0f, sphereIntersect1, sphereIntersect2);

	tangent_circle_center_next_1 = sphereIntersect1;
	tangent_circle_center_next_2 = sphereIntersect2;
	tangent_circle_radius_next = tRadius;

	p_next->tangent_circle_center_previous_1 = sphereIntersect1;
	p_next->tangent_circle_center_previous_2 = sphereIntersect2;
	p_next->tangent_circle_radius_previous = tRadius;

	tangent_circle_radius_next_cos = Math::cos(tangent_circle_radius_next);
	tangent_circle_radius_previous_cos = Math::cos(tangent_circle_radius_previous);

	if (tangent_circle_center_next_1.is_equal_approx(Vector3())) {
		tangent_circle_center_next_1 = get_orthogonal(control_point).normalized();
	}
	if (tangent_circle_center_next_2.is_equal_approx(Vector3())) {
		tangent_circle_center_next_2 = (tangent_circle_center_next_1 * -1.0f).normalized();
	}
	compute_triangles(p_next);
}

void IKDirectionLimit::set_radius(real_t p_radius) {
	radius = p_radius;
	radius_cosine = Math::cos(p_radius);
	parent_kusudama->constraint_update_notification();
}

Vector3 IKDirectionLimit::get_orthogonal(Vector3 p_vec) {
	real_t threshold = p_vec.length() * 0.6f;
	if (threshold > 0) {
		if (Math::absf(p_vec.x) <= threshold) {
			real_t inverse = 1 / Math::sqrt(p_vec.y * p_vec.y + p_vec.z * p_vec.z);
			return Vector3(0, inverse * p_vec.z, -inverse * p_vec.y);
		} else if (Math::absf(p_vec.y) <= threshold) {
			real_t inverse = 1 / Math::sqrt(p_vec.x * p_vec.x + p_vec.z * p_vec.z);
			return Vector3(-inverse * p_vec.z, 0, inverse * p_vec.x);
		}
		real_t inverse = 1.0f / Math::sqrt(p_vec.x * p_vec.x + p_vec.y * p_vec.y);
		return Vector3(inverse * p_vec.y, -inverse * p_vec.x, 0);
	}

	return Vector3();
}

Vector3 IKDirectionLimit::get_control_point() const {
	return control_point;
}

real_t IKDirectionLimit::get_radius_cosine() const {
	return radius_cosine;
}

real_t IKDirectionLimit::get_radius() const {
	return radius;
}

Vector3 IKDirectionLimit::get_closest_path_point(Ref<IKDirectionLimit> p_next, Vector3 p_input) const {
	Vector3 result = get_on_path_sequence(p_next, p_input);
	if (result.is_equal_approx(Vector3())) {
		result = closest_directional_limit(p_next, p_input);
	}
	return result;
}

Vector3 IKDirectionLimit::closest_directional_limit(Ref<IKDirectionLimit> p_next, Vector3 p_input) const {
	if (p_input.dot(control_point) > p_input.dot(p_next->control_point))
		return control_point;
	else
		return p_next->control_point;
}

Vector3 IKDirectionLimit::get_on_path_sequence(Ref<IKDirectionLimit> p_next, Vector3 p_input) const {
	Vector3 c1xc2 = control_point.cross(p_next->control_point);
	real_t c1c2fir = p_input.dot(c1xc2);
	if (c1c2fir < 0.0) {
		Vector3 c1xt1 = control_point.cross(tangent_circle_center_next_1);
		Vector3 t1xc2 = tangent_circle_center_next_1.cross(p_next->control_point);
		if (p_input.dot(c1xt1) > 0 && p_input.dot(t1xc2) > 0) {
			Ray tan1ToInput;
			tan1ToInput.position = tangent_circle_center_next_1;
			tan1ToInput.normal = p_input;
			Vector3 result;
			Plane plane = Plane(control_point, tan1ToInput.normal);
			plane.intersects_ray(p_next->control_point, (control_point - p_next->control_point).normalized(), &result);
			return (result - p_next->control_point).normalized();
		} else {
			return Vector3();
		}
	} else {
		Vector3 t2xc1 = tangent_circle_center_next_2.cross(control_point);
		Vector3 c2xt2 = p_next->control_point.cross(tangent_circle_center_next_2);
		if (p_input.dot(t2xc1) > 0 && p_input.dot(c2xt2) > 0) {
			Ray tan2ToInput;
			tan2ToInput.position = tangent_circle_center_next_2;
			tan2ToInput.normal = p_input;
			Vector3 result;

			Plane plane = Plane(control_point, tan2ToInput.normal);
			plane.intersects_ray(p_next->control_point, (control_point - p_next->control_point).normalized(), &result);
			return (result - p_next->control_point).normalized();
		} else {
			return Vector3();
		}
	}
}

void IKDirectionLimit::compute_triangles(Ref<IKDirectionLimit> p_next) {
	first_triangle_next.resize(3);
	//TODO Move normalization outside
	first_triangle_next.write[1] = tangent_circle_center_next_1.normalized();
	first_triangle_next.write[0] = get_control_point().normalized();
	first_triangle_next.write[2] = p_next->get_control_point().normalized();

	second_triangle_next.resize(3);
	second_triangle_next.write[1] = tangent_circle_center_next_2.normalized();
	second_triangle_next.write[0] = get_control_point().normalized();
	second_triangle_next.write[2] = get_control_point().normalized();
}

void IKDirectionLimit::set_control_point(Vector3 p_control_point) {
	control_point = p_control_point;
	//TODO
	// if (parent_kusudama.is_valid()) {
	// 	parent_kusudama->constraint_update_notification();
	// }
}

void IKDirectionLimit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &IKDirectionLimit::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &IKDirectionLimit::get_radius);
	ClassDB::bind_method(D_METHOD("set_control_point", "control_point"), &IKDirectionLimit::set_control_point);
	ClassDB::bind_method(D_METHOD("get_control_point"), &IKDirectionLimit::get_control_point);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "radius"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "control_point"), "set_control_point", "get_control_point");
}

Vector3 IKDirectionLimit::get_on_great_tangent_triangle(Ref<IKDirectionLimit> next, Vector3 input) const {
	Vector3 c1xc2 = get_control_point().cross(next->get_control_point());
	float c1c2fir = input.dot(c1xc2);
	if (c1c2fir < 0.0) {
		Vector3 c1xt1 = get_control_point().cross(tangent_circle_center_next_1);
		Vector3 t1xc2 = tangent_circle_center_next_1.cross(next->get_control_point());
		if (input.dot(c1xt1) > 0 && input.dot(t1xc2) > 0) {
			if (input.dot(tangent_circle_center_next_1) > tangent_circle_radius_next_cos) {
				Vector3 planeNormal = tangent_circle_center_next_1.cross(input);
				Quat rotateAboutBy = Quat(planeNormal, tangent_circle_radius_next);
				Transform xform;
				xform.basis = Basis(rotateAboutBy);
				return xform.xform(tangent_circle_center_next_1);
			} else {
				return input;
			}
		} else {
			return Vector3();
		}
	} else {
		Vector3 t2xc1 = tangent_circle_center_next_2.cross(get_control_point());
		Vector3 c2xt2 = next->get_control_point().cross(tangent_circle_center_next_2);
		if (input.dot(t2xc1) > 0 && input.dot(c2xt2) > 0) {
			if (input.dot(tangent_circle_center_next_2) > tangent_circle_radius_next_cos) {
				Vector3 planeNormal = tangent_circle_center_next_2.cross(input);
				Quat rotateAboutBy = Quat(planeNormal, tangent_circle_radius_next);
				Transform xform;
				xform.basis = Basis(rotateAboutBy);
				return xform.xform(tangent_circle_center_next_2);
			} else {
				return input;
			}
		} else {
			return Vector3();
		}
	}
}

Vector3 IKDirectionLimit::closest_to_cone(Vector3 input, Vector<bool> inBounds) const {

	if (input.dot(get_control_point()) > get_radius_cosine()) {
		inBounds.write[0] = true;
		return Vector3();
	} else {
		Vector3 axis = get_control_point().cross(input);
		Quat rotTo = Quat(axis, get_radius());
		Transform xform;
		xform.basis = Basis(rotTo);
		Vector3 result = xform.xform(get_control_point());
		inBounds.write[0] = false;
		return result;
	}
}

Vector3
IKDirectionLimit::closest_point_on_closest_cone(Ref<IKDirectionLimit> next, Vector3 input, Vector<bool> inBounds) const {
	Vector3 closestToFirst = closest_to_cone(input, inBounds);
	if (inBounds[0]) {
		return closestToFirst;
	}
	Vector3 closestToSecond = next->closest_to_cone(input, inBounds);
	if (inBounds[0]) {
		return closestToSecond;
	}

	float cosToFirst = input.dot(closestToFirst);
	float cosToSecond = input.dot(closestToSecond);

	if (cosToFirst > cosToSecond) {
		return closestToFirst;
	} else {
		return closestToSecond;
	}
}

Vector3 IKDirectionLimit::get_closest_collision(Ref<IKDirectionLimit> next, Vector3 input) const {
	Vector3 result = get_on_great_tangent_triangle(next, input);
	if (result == Vector3()) {
		Vector<bool> inBounds;
		inBounds.resize(1);
		inBounds.write[0] = false;
		result = closest_point_on_closest_cone(next, input, inBounds);
	}
	return result;
}

bool IKDirectionLimit::in_bounds_from_this_to_next(Ref<IKDirectionLimit> next, Vector3 input, Vector3 collisionPoint) const {
	bool isInBounds = false;
	Vector3 closestCollision = get_closest_collision(next, input);
	if (closestCollision == Vector3()) {
		/**
         * getClosestCollision returns null if the point is already in bounds,
         * so we set isInBounds to true.
         */
		isInBounds = true;
		collisionPoint.x = input.x;
		collisionPoint.y = input.y;
		collisionPoint.z = input.z;
	} else {
		collisionPoint.x = closestCollision.x;
		collisionPoint.y = closestCollision.y;
		collisionPoint.z = closestCollision.z;
	}
	return isInBounds;
}

void IKAxes::operator=(const IKAxes &p_axes) {
	set_basis(p_axes.basis);
	set_origin(p_axes.origin);
}

int IKAxes::get_global_chirality() {
	return get_local_chirality();
}

int IKAxes::get_local_chirality() {
	return chirality;
}

real_t IKConstraintKusudama::get_rotational_freedom() const {
	return rotational_freedom;
}

real_t IKConstraintKusudama::from_tau(real_t p_tau) const {
	real_t result = p_tau;

	if (p_tau < 0.0f) {
		result = 1.0f + p_tau;
	}
	result = Math::fmod(result, 1.0f);
	result = result * Math_PI;
	return result;
}

real_t IKConstraintKusudama::to_tau(real_t p_angle) const {
	real_t result = p_angle;
	if (p_angle < 0) {
		result = (2 * Math_PI) + p_angle;
	}
	result = Math::fmod(double(result), Math_PI * 2.0);
	return result;
}

real_t IKConstraintKusudama::signed_angle_difference(real_t p_min_angle, real_t p_base) {
	real_t d = Math::fmod(Math::absd(p_min_angle - p_base), double(Math_TAU));
	real_t r = d > Math_PI ? Math_TAU - d : d;
	real_t sign = (p_min_angle - p_base >= 0 && p_min_angle - p_base <= Math_PI) ||
								  (p_min_angle - p_base <= -Math_PI && p_min_angle - p_base >= -Math_TAU) ?
						  1.0f :
						  -1.0f;
	r *= sign;
	return r;
}

real_t IKConstraintKusudama::snap_to_twist_limits(IKAxes p_to_set, IKAxes p_limiting_axes) {

	if (!axial_constrained) {
		return 0.0f;
	}

	IKQuat alignRot = p_limiting_axes.basis.get_rotation_quat().inverse() * p_to_set.basis.get_rotation_quat();
	Vector<IKQuat> decomposition = alignRot.get_swing_twist(Vector3(0, 1, 0));
	const int32_t axis_y = 1;
	real_t angleDelta2 = Basis(decomposition[1]).get_axis(axis_y).y * -1.0f;
	angleDelta2 = to_tau(angleDelta2);
	real_t fromMinToAngleDelta = to_tau(signed_angle_difference(angleDelta2, axial_limit->get_min_twist_angle()));

	if (fromMinToAngleDelta < Math_TAU - to_tau(axial_limit->get_range())) {
		real_t distToMin = Math::absf(signed_angle_difference(angleDelta2, axial_limit->get_min_twist_angle()));
		real_t distToMax = Math::absf(signed_angle_difference(angleDelta2, Math_TAU - (to_tau(
																							   axial_limit->get_min_twist_angle()) +
																							  to_tau(axial_limit->get_range()))));
		real_t turnDiff = 1.0f;
		turnDiff *= p_limiting_axes.get_global_chirality();
		if (distToMin < distToMax) {
			turnDiff = turnDiff * (fromMinToAngleDelta);
			p_to_set.rotate_basis(Vector3(0, 1, 0), turnDiff);
			p_to_set.orthonormalize();
		} else {
			turnDiff = turnDiff * (to_tau(axial_limit->get_range()) - (Math_TAU - fromMinToAngleDelta));
			p_to_set.rotate_basis(Vector3(0, 1, 0), turnDiff);
			p_to_set.orthonormalize();
		}
		return turnDiff < 0 ? turnDiff * -1 : turnDiff;
	} else {
		return 0.f;
	}
	return 0.f;
}

void IKConstraintKusudama::update_tangent_radii() {
	for (int direction_limit_i = 0; direction_limit_i < direction_limits.size(); direction_limit_i++) {
		Ref<IKDirectionLimit> next;
		if (direction_limit_i < direction_limits.size() - 1) {
			next = direction_limits[direction_limit_i + 1];
		}
		if (next.is_null()) {
			continue;
		}
		Ref<IKDirectionLimit> direction_limit = direction_limits[direction_limit_i];
		direction_limit->update_tangent_handles(next);
	}
}

void IKConstraintKusudama::constraint_update_notification() {
	update_tangent_radii();
	update_rotational_freedom();
}

void QCP::set_max_iterations(int p_max) {
	max_iterations = p_max;
}

void QCP::set(PoolVector3Array p_target, PoolVector3Array p_moved) {
	moved = p_target;
	target = p_moved;
	rmsd_calculated = false;
	transformation_calculated = false;
}

void QCP::set(PoolVector3Array p_moved, PoolVector3Array p_target, PoolRealArray p_weight, bool p_translate) {
	target = p_target;
	moved = p_moved;
	weight = p_weight;
	rmsd_calculated = false;
	transformation_calculated = false;

	if (p_translate) {
		get_weighted_center(moved, p_weight, moved_center);
		wsum = 0.0f; //set wsum to 0 so we don't float up.
		get_weighted_center(target, p_weight, target_center);
		translate(moved_center * -1.f, moved);
		translate(target_center * -1.f, target);
	} else {
		if (!p_weight.empty()) {
			for (int i = 0; i < p_weight.size(); i++) {
				wsum += p_weight[i];
			}
		} else {
			wsum = p_moved.size();
		}
	}
}

float QCP::get_rmsd() {
	if (!rmsd_calculated) {
		calc_rmsd(moved, target);
		rmsd_calculated = true;
	}
	return rmsd;
}

IKQuat
QCP::weighted_superpose(PoolVector3Array p_moved, PoolVector3Array p_target, PoolRealArray p_weight, bool p_translate) {
	set(p_moved, p_target, p_weight, p_translate);
	Quat result = get_rotation();
	return result;
}

IKQuat QCP::get_rotation() {
	get_rmsd();
	IKQuat result;
	if (!transformation_calculated) {
		result = calc_rotation();
		transformation_calculated = true;
	}
	return result;
}

void QCP::calc_rmsd(PoolVector3Array p_x, PoolVector3Array p_y) {
	//QCP doesn't handle alignment of single values, so if we only have one point
	//we just compute regular distance.
	if (p_x.size() == 1) {
		rmsd = p_x[0].distance_to(p_y[0]);
		rmsd_calculated = true;
	} else {
		inner_product(p_y, p_x);
		calc_rmsd(wsum);
	}
}

void QCP::inner_product(PoolVector3Array p_coords1, PoolVector3Array p_coords2) {
	real_t x1, x2, y1, y2, z1, z2;
	real_t g1 = 0.0f, g2 = 0.0f;

	Sxx = 0;
	Sxy = 0;
	Sxz = 0;
	Syx = 0;
	Syy = 0;
	Syz = 0;
	Szx = 0;
	Szy = 0;
	Szz = 0;

	if (!weight.empty()) {
		for (int i = 0; i < p_coords1.size(); i++) {
			x1 = weight[i] * p_coords1[i].x;
			y1 = weight[i] * p_coords1[i].y;
			z1 = weight[i] * p_coords1[i].z;

			g1 += x1 * p_coords1[i].x + y1 * p_coords1[i].y + z1 * p_coords1[i].z;

			x2 = p_coords2[i].x;
			y2 = p_coords2[i].y;
			z2 = p_coords2[i].z;

			g2 += weight[i] * (x2 * x2 + y2 * y2 + z2 * z2);

			Sxx += (x1 * x2);
			Sxy += (x1 * y2);
			Sxz += (x1 * z2);

			Syx += (y1 * x2);
			Syy += (y1 * y2);
			Syz += (y1 * z2);

			Szx += (z1 * x2);
			Szy += (z1 * y2);
			Szz += (z1 * z2);
		}
	} else {
		for (int i = 0; i < p_coords1.size(); i++) {
			g1 += p_coords1[i].x * p_coords1[i].x + p_coords1[i].y * p_coords1[i].y + p_coords1[i].z * p_coords1[i].z;
			g2 += p_coords2[i].x * p_coords2[i].x + p_coords2[i].y * p_coords2[i].y + p_coords2[i].z * p_coords2[i].z;

			Sxx += p_coords1[i].x * p_coords2[i].x;
			Sxy += p_coords1[i].x * p_coords2[i].y;
			Sxz += p_coords1[i].x * p_coords2[i].z;

			Syx += p_coords1[i].y * p_coords2[i].x;
			Syy += p_coords1[i].y * p_coords2[i].y;
			Syz += p_coords1[i].y * p_coords2[i].z;

			Szx += p_coords1[i].z * p_coords2[i].x;
			Szy += p_coords1[i].z * p_coords2[i].y;
			Szz += p_coords1[i].z * p_coords2[i].z;
		}
	}

	e0 = (g1 + g2) * 0.5f;
}

int QCP::calc_rmsd(real_t p_len) {
	real_t Sxx2 = Sxx * Sxx;
	real_t Syy2 = Syy * Syy;
	real_t Szz2 = Szz * Szz;

	real_t Sxy2 = Sxy * Sxy;
	real_t Syz2 = Syz * Syz;
	real_t Sxz2 = Sxz * Sxz;

	real_t Syx2 = Syx * Syx;
	real_t Szy2 = Szy * Szy;
	real_t Szx2 = Szx * Szx;

	real_t SyzSzymSyySzz2 = 2.0f * (Syz * Szy - Syy * Szz);
	real_t Sxx2Syy2Szz2Syz2Szy2 = Syy2 + Szz2 - Sxx2 + Syz2 + Szy2;

	real_t c2 = -2.0f * (Sxx2 + Syy2 + Szz2 + Sxy2 + Syx2 + Sxz2 + Szx2 + Syz2 + Szy2);
	real_t c1 = 8.0f * (Sxx * Syz * Szy + Syy * Szx * Sxz + Szz * Sxy * Syx - Sxx * Syy * Szz - Syz * Szx * Sxy -
							   Szy * Syx * Sxz);

	SxzpSzx = Sxz + Szx;
	SyzpSzy = Syz + Szy;
	SxypSyx = Sxy + Syx;
	SyzmSzy = Syz - Szy;
	SxzmSzx = Sxz - Szx;
	SxymSyx = Sxy - Syx;
	SxxpSyy = Sxx + Syy;
	SxxmSyy = Sxx - Syy;

	real_t Sxy2Sxz2Syx2Szx2 = Sxy2 + Sxz2 - Syx2 - Szx2;

	real_t c0 = Sxy2Sxz2Syx2Szx2 * Sxy2Sxz2Syx2Szx2 +
				(Sxx2Syy2Szz2Syz2Szy2 + SyzSzymSyySzz2) * (Sxx2Syy2Szz2Syz2Szy2 - SyzSzymSyySzz2) +
				(-(SxzpSzx) * (SyzmSzy) + (SxymSyx) * (SxxmSyy - Szz)) *
						(-(SxzmSzx) * (SyzpSzy) + (SxymSyx) * (SxxmSyy + Szz)) +
				(-(SxzpSzx) * (SyzpSzy) - (SxypSyx) * (SxxpSyy - Szz)) *
						(-(SxzmSzx) * (SyzmSzy) - (SxypSyx) * (SxxpSyy + Szz)) +
				(+(SxypSyx) * (SyzpSzy) + (SxzpSzx) * (SxxmSyy + Szz)) *
						(-(SxymSyx) * (SyzmSzy) + (SxzpSzx) * (SxxpSyy + Szz)) +
				(+(SxypSyx) * (SyzmSzy) + (SxzmSzx) * (SxxmSyy - Szz)) *
						(-(SxymSyx) * (SyzpSzy) + (SxzmSzx) * (SxxpSyy - Szz));

	mxEigenV = e0;

	int i;
	for (i = 1; i < (max_iterations + 1); ++i) {
		real_t oldg = mxEigenV;
		real_t Y = 1.0f / mxEigenV;
		real_t Y2 = Y * Y;
		real_t delta = ((((Y * c0 + c1) * Y + c2) * Y2 + 1) / ((Y * c1 + 2 * c2) * Y2 * Y + 4));
		mxEigenV -= delta;

		if (Math::absd(mxEigenV - oldg) < Math::absd(eval_prec * mxEigenV)) {
			break;
		}
	}

	/*if (i == max_iterations) {
        System.out.println(String.format("More than %d iterations needed!", i));
    } else {
        System.out.println(String.format("%d iterations needed!", i));
    }*/

	/*
     * the fabs() is to guard against extremely small, but *negative*
     * numbers due to floating point error
     */
	rmsd = Math::sqrt(Math::absd(2.0f * (e0 - mxEigenV) / p_len));

	return 1;
}

IKQuat QCP::calc_rotation() {

	//QCP doesn't handle single targets, so if we only have one point and one target, we just rotate by the angular distance between them
	if (moved.size() == 1) {
		IKQuat single_moved;
		single_moved.set_euler(moved[0]);
		IKQuat single_target;
		single_target.set_euler(target[0]);
		return IKQuat(single_moved * single_target);
	} else {

		real_t a11 = SxxpSyy + Szz - mxEigenV;
		real_t a12 = SyzmSzy;
		real_t a13 = -SxzmSzx;
		real_t a14 = SxymSyx;
		real_t a21 = SyzmSzy;
		real_t a22 = SxxmSyy - Szz - mxEigenV;
		real_t a23 = SxypSyx;
		real_t a24 = SxzpSzx;
		real_t a31 = a13;
		real_t a32 = a23;
		real_t a33 = Syy - Sxx - Szz - mxEigenV;
		real_t a34 = SyzpSzy;
		real_t a41 = a14;
		real_t a42 = a24;
		real_t a43 = a34;
		real_t a44 = Szz - SxxpSyy - mxEigenV;
		real_t a3344_4334 = a33 * a44 - a43 * a34;
		real_t a3244_4234 = a32 * a44 - a42 * a34;
		real_t a3243_4233 = a32 * a43 - a42 * a33;
		real_t a3143_4133 = a31 * a43 - a41 * a33;
		real_t a3144_4134 = a31 * a44 - a41 * a34;
		real_t a3142_4132 = a31 * a42 - a41 * a32;
		real_t q1 = a22 * a3344_4334 - a23 * a3244_4234 + a24 * a3243_4233;
		real_t q2 = -a21 * a3344_4334 + a23 * a3144_4134 - a24 * a3143_4133;
		real_t q3 = a21 * a3244_4234 - a22 * a3144_4134 + a24 * a3142_4132;
		real_t q4 = -a21 * a3243_4233 + a22 * a3143_4133 - a23 * a3142_4132;

		real_t qsqr = q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4;

		/*
         * The following code tries to calculate another column in the adjoint
         * matrix when the norm of the current column is too small. Usually this
         * commented block will never be activated. To be absolutely safe this
         * should be uncommented, but it is most likely unnecessary.
         */
		if (qsqr < evec_prec) {
			q1 = a12 * a3344_4334 - a13 * a3244_4234 + a14 * a3243_4233;
			q2 = -a11 * a3344_4334 + a13 * a3144_4134 - a14 * a3143_4133;
			q3 = a11 * a3244_4234 - a12 * a3144_4134 + a14 * a3142_4132;
			q4 = -a11 * a3243_4233 + a12 * a3143_4133 - a13 * a3142_4132;
			qsqr = q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4;

			if (qsqr < evec_prec) {
				real_t a1324_1423 = a13 * a24 - a14 * a23, a1224_1422 = a12 * a24 - a14 * a22;
				real_t a1223_1322 = a12 * a23 - a13 * a22, a1124_1421 = a11 * a24 - a14 * a21;
				real_t a1123_1321 = a11 * a23 - a13 * a21, a1122_1221 = a11 * a22 - a12 * a21;

				q1 = a42 * a1324_1423 - a43 * a1224_1422 + a44 * a1223_1322;
				q2 = -a41 * a1324_1423 + a43 * a1124_1421 - a44 * a1123_1321;
				q3 = a41 * a1224_1422 - a42 * a1124_1421 + a44 * a1122_1221;
				q4 = -a41 * a1223_1322 + a42 * a1123_1321 - a43 * a1122_1221;
				qsqr = q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4;

				if (qsqr < evec_prec) {
					q1 = a32 * a1324_1423 - a33 * a1224_1422 + a34 * a1223_1322;
					q2 = -a31 * a1324_1423 + a33 * a1124_1421 - a34 * a1123_1321;
					q3 = a31 * a1224_1422 - a32 * a1124_1421 + a34 * a1122_1221;
					q4 = -a31 * a1223_1322 + a32 * a1123_1321 - a33 * a1122_1221;
					qsqr = q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4;

					if (qsqr < evec_prec) {
						/*
                         * if qsqr is still too small, return the identity rotation
                         */
						return IKQuat();
					}
				}
			}
		}
		//prenormalize the result to avoid floating point errors.
		real_t min = q1;
		min = q2 < min ? q2 : min;
		min = q3 < min ? q3 : min;
		min = q4 < min ? q4 : min;
		Quat result = Quat(q1 / min, q2 / min, q3 / min, q4 / min).normalized();
		return IKQuat(result);
	}
}

float QCP::get_rmsd(PoolVector3Array p_fixed, PoolVector3Array p_moved) {
	set(p_moved, p_fixed);
	return get_rmsd();
}

void QCP::translate(Vector3 p_trans, PoolVector3Array p_x) {
	for (int32_t trans_i = 0; trans_i < p_x.size(); trans_i++) {
		p_x.write()[trans_i] += p_trans;
	}
}

Vector3 QCP::get_weighted_center(PoolVector3Array p_to_center, PoolRealArray p_weight, Vector3 p_center) {

	if (!p_weight.empty()) {
		for (int i = 0; i < p_to_center.size(); i++) {
			p_center = p_center * p_to_center[i] + Vector3(p_weight[i], p_weight[i], p_weight[i]);
			wsum += p_weight[i];
		}

		p_center = p_center / wsum;
	} else {
		for (int i = 0; i < p_to_center.size(); i++) {
			p_center = p_center + p_to_center[i];
			wsum++;
		}
		p_center = p_center / wsum;
	}

	return p_center;
}

Vector3 QCP::get_translation() {
	return target_center - moved_center;
}

void SkeletonIKConstraints::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_constraint_count", "constraint_count"),
			&SkeletonIKConstraints::set_constraint_count);
	ClassDB::bind_method(D_METHOD("get_constraint_count"), &SkeletonIKConstraints::get_constraint_count);

	ClassDB::bind_method(D_METHOD("set_bone_names", "bone_names"), &SkeletonIKConstraints::set_bone_names);
	ClassDB::bind_method(D_METHOD("get_bone_names"), &SkeletonIKConstraints::get_bone_names);

	ADD_PROPERTY(PropertyInfo(Variant::POOL_STRING_ARRAY, "bone_names", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_INTERNAL), "set_bone_names", "get_bone_names");
}

void SkeletonIKConstraints::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::INT, "constraint_count", PROPERTY_HINT_RANGE, "0,16384,1,or_greater"));
	for (int i = 0; i < constraints.size(); i++) {
		PropertyHint hint = PROPERTY_HINT_NONE;
		String hint_string = "";
		if (get_bone_names().size()) {
			hint_string = "--,";
			for (int i = 0; i < get_bone_names().size(); i++) {
				if (i > 0)
					hint_string += ",";
				hint_string += get_bone_names()[i];
			}
			hint = PROPERTY_HINT_ENUM;
		}
		p_list->push_back(PropertyInfo(Variant::STRING, "constraints/" + itos(i) + "/chain_item", hint, hint_string));
		p_list->push_back(
				PropertyInfo(Variant::OBJECT, "constraints/" + itos(i) + "/constraint", PROPERTY_HINT_RESOURCE_TYPE,
						"IKConstraintKusudama"));
	}
}

bool SkeletonIKConstraints::_get(const StringName &p_name, Variant &r_ret) const {

	String name = p_name;
	if (name == "constraint_count") {
		r_ret = get_constraint_count();
		return true;
	} else if (name.begins_with("constraints/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		if (what == "chain_item") {
			r_ret = get_chain_item(index);
			return true;
		} else if (what == "constraint") {
			r_ret = get_constraint(index);
			return true;
		}
	}
	return false;
}

bool SkeletonIKConstraints::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;
	if (name == "constraint_count") {
		set_constraint_count(p_value);
		return true;
	} else if (name.begins_with("constraints/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		if (what == "chain_item") {
			set_chain_item(index, p_value);
			return true;
		} else if (what == "constraint") {
			set_constraint(index, p_value);
			return true;
		}
	}
	return false;
}

Ref<IKConstraintKusudama> SkeletonIKConstraints::get_constraint(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, constraints.size(), Ref<IKConstraintKusudama>())
	ERR_FAIL_COND_V(constraints[p_index].is_null(), Ref<IKConstraintKusudama>());
	return constraints[p_index]->constraint;
}

void SkeletonIKConstraints::set_constraint(int32_t p_index, Ref<IKConstraintKusudama> p_constraint) {
	ERR_FAIL_INDEX(p_index, constraints.size())
	if (constraints.write[p_index].is_null()) {
		constraints.write[p_index].instance();
	}
	constraints.write[p_index]->constraint = Ref<IKConstraintKusudama>(p_constraint);
	_change_notify();
	emit_changed();
}

PoolStringArray SkeletonIKConstraints::get_bone_names() const {
	return bone_names;
}

void SkeletonIKConstraints::set_bone_names(PoolStringArray p_bones) {
	bone_names = p_bones;
	_change_notify();
	emit_changed();
}

StringName SkeletonIKConstraints::get_chain_item(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, constraints.size(), "")
	ERR_FAIL_COND_V(constraints[p_index].is_null(), "");
	return constraints[p_index]->chain_item_name;
}

void SkeletonIKConstraints::set_chain_item(int32_t p_index, StringName p_value) {
	ERR_FAIL_INDEX(p_index, constraints.size())
	if (constraints.write[p_index].is_null()) {
		constraints.write[p_index].instance();
	}
	constraints.write[p_index]->chain_item_name = p_value;
	_change_notify();
	emit_changed();
}

int32_t SkeletonIKConstraints::get_constraint_count() const {
	return constraints.size();
}

void SkeletonIKConstraints::set_constraint_count(int32_t p_value) {
	constraints.resize(p_value);
	_change_notify();
	emit_changed();
}

void IKConstraintKusudama::set_axes_to_snapped(IKAxes p_to_set, IKAxes p_limiting_axes, float p_cos_half_angle_dampen) {
	if (!p_limiting_axes.is_equal_approx(Transform())) {
		if (orientation_constrained) {
			set_axes_to_orientation_snap(p_to_set, p_limiting_axes, p_cos_half_angle_dampen);
		}
		if (axial_constrained) {
			snap_to_twist_limits(p_to_set, p_limiting_axes);
		}
	}
}

void IKConstraintKusudama::set_axes_to_orientation_snap(IKAxes p_to_set, IKAxes p_limiting_axes,
		float p_cos_half_angle_dampen) {
	Vector<real_t> inBounds;
	inBounds.push_back(1.f);
	bone_ray.position = Vector3(p_to_set.origin);

	// toSet.y_().getScaledTo(attachedTo.boneHeight);
	bone_ray.normal = p_to_set.basis.get_axis(CMDDInverseKinematic::y_axis) * attached_to->get_bone_height();
	Vector3 in_limits = pointInLimits(bone_ray.normal, inBounds, limiting_axes);

	// if (inBounds[0] == -1 && inLimits != Vector3()) {
	// 	constrained_ray.position = bone_ray.position;
	// 	constrained_ray.normal = inLimits;
	// 	Quat rectifiedRot = Quat(bone_ray.heading(), constrained_ray.heading());
	// 	toSet.basis *= rectifiedRot;
	// }
}

void IKConstraintKusudama::set_axes_to_returnful(IKAxes p_global_xform, IKAxes p_to_set, IKAxes p_limiting_axes,
		real_t p_cos_half_angle_dampen, real_t p_angle_dampen) {
	if (!p_limiting_axes.is_equal_approx(Transform()) && pain > 0.0f) {
		const int32_t y_axis = 1;
		if (orientation_constrained) {
			Vector3 origin = p_to_set.origin;
			Vector3 inPoint = p_to_set.basis.get_axis(y_axis);
			Vector3 pathPoint = point_on_path_sequence(p_global_xform, inPoint, p_limiting_axes);
			inPoint -= origin;
			pathPoint -= origin;
			IKQuat toClamp = Quat(inPoint, pathPoint);
			toClamp.clamp_to_quadrance_angle(p_cos_half_angle_dampen);
			p_to_set.basis.rotate(toClamp);
		}
		if (axial_constrained) {
			real_t angleToTwistMid = angle_to_twist_center(p_global_xform, p_to_set, p_limiting_axes);
			real_t clampedAngle = CLAMP(angleToTwistMid, -p_angle_dampen, p_angle_dampen);
			p_to_set.basis = Quat(p_global_xform.basis.get_axis(y_axis), clampedAngle);
		}
	}
}

real_t IKConstraintKusudama::angle_to_twist_center(IKAxes p_global_xform, IKAxes p_to_set, IKAxes p_limiting_axes) {
	if (!axial_constrained) {
		return 0.0f;
	}
	IKAxes limiting_axes_global = p_global_xform * limiting_axes;
	IKAxes to_set_global = p_global_xform * p_to_set;
	IKQuat align_rot =
			limiting_axes_global.basis.get_rotation_quat().inverse() * to_set_global.basis.get_rotation_quat();
	Vector<IKQuat> decomposition = align_rot.get_swing_twist(Vector3(0, 1, 0));
	const int32_t axis_y = 1;
	real_t angle_delta_2 = Basis(decomposition[1]).get_axis(axis_y).y * -1.0f;
	angle_delta_2 = to_tau(angle_delta_2);
	real_t dist_to_mid = signed_angle_difference(angle_delta_2,
			Math_TAU - (to_tau(axial_limit->get_min_twist_angle()) + (to_tau((axial_limit->get_range()) / 2.0f))));
	return dist_to_mid;
}

Vector3
IKConstraintKusudama::point_on_path_sequence(IKAxes p_global_xform, Vector3 p_in_point, IKAxes p_limiting_axes) {
	real_t closestPointDot = 0.0f;
	Vector3 point = p_limiting_axes.origin;
	point.normalize();
	Vector3 result = point;

	if (direction_limits.size() == 1) {
		Ref<IKDirectionLimit> direction_limit = direction_limits[0];
		result = direction_limit->control_point;
	} else {
		for (int direction_limit_i = 0; direction_limit_i < direction_limits.size() - 1; direction_limit_i++) {
			Ref<IKDirectionLimit> next_direction = direction_limits[direction_limit_i + 1];
			Ref<IKDirectionLimit> direction = direction_limits[direction_limit_i];
			Vector3 closest_path_point = direction->get_closest_path_point(next_direction, point);
			real_t close_dot = closest_path_point.dot(point);
			if (close_dot > closestPointDot) {
				result = closest_path_point;
				closestPointDot = close_dot;
			}
		}
	}

	return p_global_xform.xform(result);
}

real_t IKConstraintKusudama::get_pain() {
	return pain;
}

void IKConstraintKusudama::set_limiting_axes(const IKAxes &p_limiting_axes) {
	limiting_axes = p_limiting_axes;
}

IKAxes IKConstraintKusudama::get_limiting_axes() const {
	return limiting_axes;
}

bool IKConstraintKusudama::is_in_limits_(const Vector3 p_global_point) const {
	return false;
}

void IKConstraintKusudama::update_rotational_freedom() {
	float axialConstrainedHyperArea = axial_constrained ? (to_tau(axial_limit->get_range()) / Math_TAU) : 1.0f;
	// quick and dirty solution (should revisit);
	float total_limit_cone_surface_area_ratio = 0.0f;
	for (int32_t limit_cone_i = 0; limit_cone_i < direction_limits.size(); limit_cone_i++) {
		Ref<IKDirectionLimit> direction_limit = direction_limits[limit_cone_i];
		total_limit_cone_surface_area_ratio += (direction_limit->get_radius() * 2.0f) / Math_TAU;
	}
	rotational_freedom = axialConstrainedHyperArea *
						 (orientation_constrained ? MIN(total_limit_cone_surface_area_ratio, 1.0f) : 1.0f);
}

Vector3 Ray::set_to_inverted_tip(Vector3 p_vec) {
	p_vec.x = (position.x - normal.x) + position.x;
	p_vec.y = (position.y - normal.y) + position.y;
	p_vec.z = (position.z - normal.z) + position.z;
	return p_vec;
}

void Ray::elongate(real_t p_amount) {
	Vector3 midPoint = position + normal + Vector3(0.5f, 0.5f, 0.5f);
	Vector3 p1Heading = position - midPoint;
	Vector3 p2Heading = normal - midPoint;
	Vector3 p1Add = p1Heading.normalized() * p_amount;
	Vector3 p2Add = p2Heading.normalized() * p_amount;

	position = p1Heading + p1Add + midPoint;
	normal = p2Heading + p2Add + midPoint;
}

int Ray::intersects_sphere(Vector3 sphereCenter, double radius, Vector3 S1, Vector3 S2) {
	Vector3 tp1 = position - sphereCenter;
	Vector3 tp2 = normal - sphereCenter;
	int result = intersects_sphere(tp1, tp2, radius, S1, S2);
	S1 = S1 + sphereCenter;
	S2 = S2 + sphereCenter;
	return result;
}

int Ray::intersects_sphere(Vector3 rp1, Vector3 rp2, double radius, Vector3 &S1, Vector3 &S2) {
	Vector3 direction = rp2 - rp1;
	Vector3 e = direction; // e=ray.dir
	e.normalize(); // e=g/|g|
	Vector3 h = rp1;
	h = Vector3(0.f, 0.f, 0.f);
	h = h - rp1; // h=r.o-c.M
	double lf = e.dot(h); // lf=e.h
	double radpow = radius * radius;
	double hdh = h.length_squared();
	double lfpow = lf * lf;
	double s = radpow - hdh + lfpow; // s=r^2-h^2+lf^2
	if (s < 0.0) {
		return 0; // no intersection points ?
	}
	s = Math::sqrt(s); // s=sqrt(r^2-h^2+lf^2)

	int result = 0;
	if (lf < s) { // S1 behind A ?
		if (lf + s >= 0) { // S2 before A ?}
			s = -s; // swap S1 <-> S2}
			result = 1; // one intersection point
		}
	} else
		result = 2; // 2 intersection points

	S1 = e * (lf - s);
	S1 = S1 + rp1; // S1=A+e*(lf-s)
	S2 = e * (lf + s);
	S2 = S2 + rp1; // S2=A+e*(lf+s)

	// only for testing

	return result;
}

Vector<IKQuat> IKQuat::get_swing_twist(Vector3 p_axis) {
	Vector3 euler = get_euler();
	const real_t d = Vector3(
			euler.x,
			euler.y,
			euler.z)
							 .dot(Vector3(p_axis.x, p_axis.y, p_axis.z));
	set(p_axis.x * d, p_axis.y * d, p_axis.z * d, w);
	normalize();
	if (d < 0) {
		operator*(-1.0f);
	}

	IKQuat swing = IKQuat(-x, -y, -z, w);
	swing = operator*(swing);

	Vector<IKQuat> result;
	result.resize(2);
	result.write[0] = swing;
	result.write[1] = IKQuat(x, y, z, w);
	return result;
}

void IKQuat::clamp_to_quadrance_angle(real_t p_cos_half_angle) {
	real_t new_coeff = 1.0f - (p_cos_half_angle * p_cos_half_angle);
	real_t current_coeff = y * y + z * z + w * w;
	if (new_coeff > current_coeff) {
		return;
	} else {
		x = x < 0.0 ? -p_cos_half_angle : p_cos_half_angle;
		real_t composite_coeff = Math::sqrt(new_coeff / current_coeff);
		y *= composite_coeff;
		z *= composite_coeff;
		w *= composite_coeff;
	}
}

void IKQuat::clamp_to_angle(real_t p_angle) {
	real_t cos_half_angle = Math::cos(0.5f * p_angle);
	clamp_to_quadrance_angle(cos_half_angle);
}

int CMDDInverseKinematic::Chain::get_default_iterations() const {
	return ik_iterations;
}

void CMDDInverseKinematic::ChainTarget::set_target_priorities(float p_x_priority, float p_y_priority, float p_z_priority) {
	bool x_dir = p_x_priority > 0 ? true : false;
	bool y_dir = p_y_priority > 0 ? true : false;
	bool z_dir = p_z_priority > 0 ? true : false;
	mode_code = 0;
	if (x_dir) mode_code += XDir;
	if (y_dir) mode_code += YDir;
	if (z_dir) mode_code += ZDir;

	sub_target_count = 1;
	if ((mode_code & 1) != 0) sub_target_count++;
	if ((mode_code & 2) != 0) sub_target_count++;
	if ((mode_code & 4) != 0) sub_target_count++;

	x_priority = p_x_priority;
	y_priority = p_y_priority;
	z_priority = p_z_priority;
	chain_item->parent_item->rootwardly_update_falloff_cache_from(for_bone());
}

real_t CMDDInverseKinematic::ChainTarget::get_depth_falloff() const {
	return depthFalloff;
}

void CMDDInverseKinematic::ChainTarget::set_depth_falloff(float depth) {
	depthFalloff = depth;
	chain_item->parent_item->rootwardly_update_falloff_cache_from(for_bone());
}

void CMDDInverseKinematic::ChainTarget::disable() {
	enabled = false;
}

void CMDDInverseKinematic::ChainTarget::enable() {
	enabled = true;
}

void CMDDInverseKinematic::ChainTarget::toggle() {
	if (is_enabled()) {
		disable();
	} else {
		enable();
	}
}

bool CMDDInverseKinematic::ChainTarget::is_enabled() const {
	return enabled;
}

int CMDDInverseKinematic::ChainTarget::get_subtarget_count() {
	return sub_target_count;
}

uint8_t CMDDInverseKinematic::ChainTarget::get_mode_code() const {
	return mode_code;
}

real_t CMDDInverseKinematic::ChainTarget::get_x_priority() const {
	return x_priority;
}

real_t CMDDInverseKinematic::ChainTarget::get_y_priority() const {
	return y_priority;
}

real_t CMDDInverseKinematic::ChainTarget::get_z_priority() const {
	return z_priority;
}

IKAxes CMDDInverseKinematic::ChainTarget::get_axes() const {
	return IKAxes(chain_item->axes->local_transform.basis, chain_item->axes->local_transform.origin);
}

void CMDDInverseKinematic::ChainTarget::align_to_axes(IKAxes inAxes) {
	//TODO
	//axes.alignGlobalsTo(inAxes);
}

void CMDDInverseKinematic::ChainTarget::translate_global(Vector3 location) {
	//TODO
	//Remove
	chain_item->axes->local_transform.origin *= location;
}

void CMDDInverseKinematic::ChainTarget::translate(Vector3 location) {
	chain_item->axes->local_transform.origin *= location;
}

Vector3 CMDDInverseKinematic::ChainTarget::get_location() {
	return chain_item->axes->local_transform.origin;
}

Ref<CMDDInverseKinematic::ChainItem> CMDDInverseKinematic::ChainTarget::for_bone() {
	return chain_item;
}

void CMDDInverseKinematic::ChainTarget::removal_notification() {
	for (int32_t target_i = 0; target_i < child_targets.size(); target_i++) {
		child_targets.write[target_i]->set_parent_target(get_parent_target());
	}
}

void IKConstraintKusudama::set_pain(real_t p_amount) {
	pain = p_amount;
	if (attached_to == NULL || attached_to->parent_armature == NULL) {
		return;
	}
	Ref<CMDDInverseKinematic::Chain> s = attached_to->parent_armature;
	if (s == NULL) {
		return;
	}
	Ref<CMDDInverseKinematic::ChainItem> wb = s->chain_root.find_child(attached_to->bone);
	if (wb == NULL) {
		return;
	}
	wb->update_cos_dampening();
}

bool IKConstraintKusudama::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;
	if (name == "direction_limit_count") {
		int32_t initial_size = direction_limits.size();
		int32_t new_size = p_value;
		direction_limits.resize(p_value);
		if (initial_size < new_size) {
			for (int32_t direction_limit_i = initial_size; direction_limit_i < new_size; direction_limit_i++) {
				Ref<IKDirectionLimit> direction_limit;
				direction_limit.instance();
				direction_limit->initialize(Vector3(), 0.0f, this);
				direction_limits.write[direction_limit_i] = direction_limit;
			}
		}
		_change_notify();
		emit_changed();
		return true;
	} else if (name.begins_with("direction_limits/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		if (what == "control_point") {
			Ref<IKDirectionLimit> direction_limit = direction_limits[index];
			direction_limit->set_control_point(p_value);
			return true;
		} else if (what == "radius") {
			Ref<IKDirectionLimit> direction_limit = direction_limits[index];
			direction_limit->set_radius(p_value);
			return true;
		}
	}
	return false;
}

void CMDDInverseKinematic::ChainItem::rootwardly_update_falloff_cache_from(Ref<CMDDInverseKinematic::ChainItem> p_current) {
	Ref<ChainItem> current = p_current;
	while (current.is_valid()) {
		current->parent_armature->create_headings_arrays();
		current = current->parent_item;
	}
}

void CMDDInverseKinematic::Chain::recursively_create_penalty_array(Ref<CMDDInverseKinematic::Chain> from,
		Vector<Vector<real_t> > &r_weight_array,
		Vector<Ref<CMDDInverseKinematic::ChainItem> >

				pin_sequence,
		real_t current_falloff) {
	if (current_falloff == 0) {
		return;
	} else {
		for (int32_t target_i = 0; target_i < from->targets.size(); target_i++) {
			Ref<ChainTarget> target = from->targets[target_i];
			if (target.is_valid()) {
				Vector<real_t> inner_weight_array;
				uint8_t mode_code = target->get_mode_code();
				inner_weight_array.push_back(target->get_target_weight() * current_falloff);
				float max_target_weight = 0.0f;
				if ((mode_code & ChainTarget::XDir) != 0)
					max_target_weight = MAX(max_target_weight, target->get_x_priority());
				if ((mode_code & ChainTarget::YDir) != 0)
					max_target_weight = MAX(max_target_weight, target->get_y_priority());
				if ((mode_code & ChainTarget::ZDir) != 0)
					max_target_weight = MAX(max_target_weight, target->get_z_priority());

				if (max_target_weight == 0.0f) max_target_weight = 1.0f;

				max_target_weight = 1.0f;

				if ((mode_code & ChainTarget::XDir) != 0) {
					float sub_target_weight = target->get_target_weight() * (target->get_x_priority() / max_target_weight) * current_falloff;
					inner_weight_array.push_back(sub_target_weight);
					inner_weight_array.push_back(sub_target_weight);
				}
				if ((mode_code & ChainTarget::YDir) != 0) {
					float sub_target_weight = target->get_target_weight() * (target->get_y_priority() / max_target_weight) * current_falloff;
					inner_weight_array.push_back(sub_target_weight);
					inner_weight_array.push_back(sub_target_weight);
				}
				if ((mode_code & ChainTarget::ZDir) != 0) {
					float sub_target_weight = target->get_target_weight() * (target->get_z_priority() / max_target_weight) * current_falloff;
					inner_weight_array.push_back(sub_target_weight);
					inner_weight_array.push_back(sub_target_weight);
				}
				pin_sequence.push_back(target->for_bone());
				r_weight_array.push_back(inner_weight_array);
			}
			real_t this_falloff = target.is_null() ? 1.0f : target->get_depth_falloff();
			recursively_create_penalty_array(this, r_weight_array, pin_sequence, current_falloff * this_falloff);
		}
	}
}

void CMDDInverseKinematic::Chain::create_headings_arrays() {
	Vector<Vector<real_t> > penalty_array;
	Vector<Ref<ChainItem> > target_sequence;
	recursively_create_penalty_array(this, penalty_array, target_sequence, 1.0f);
	Vector<Ref<ChainItem> > target_bones;
	target_bones.resize(target_sequence.size());
	int total_headings = 0;
	for (int32_t penalty_i = 0; penalty_i < penalty_array.size(); penalty_i++) {
		total_headings += penalty_array[penalty_i].size();
	}
	for (int pin_i = 0; pin_i < target_sequence.size(); pin_i++) {
		target_bones.write[pin_i] = target_sequence[pin_i];
	}
	localized_effector_headings.resize(total_headings);
	localized_target_headings.resize(total_headings);
	weights.resize(total_headings);
	int current_heading = 0;
	for (int32_t array_i = 0; array_i < penalty_array.size(); array_i++) {
		for (int32_t penalty_i = 0; penalty_i < penalty_array[array_i].size(); penalty_i++) {
			weights.write()[current_heading] = penalty_array[array_i][penalty_i];
			localized_effector_headings[current_heading] = Vector3();
			localized_target_headings[current_heading] = Vector3();
			current_heading++;
		}
	}
}

bool IKConstraintKusudama::_get(const StringName &p_name, Variant &r_ret) const {

	String name = p_name;
	if (name == "direction_limit_count") {
		r_ret = direction_limits.size();
		return true;
	} else if (name.begins_with("direction_limits/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		if (what == "control_point") {
			Ref<IKDirectionLimit> direction_limit = direction_limits[index];
			r_ret = direction_limit->get_control_point();
			return true;
		} else if (what == "radius") {
			Ref<IKDirectionLimit> direction_limit = direction_limits[index];
			r_ret = direction_limit->get_radius();
			return true;
		}
	}
	return false;
}

void IKConstraintKusudama::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::INT, "direction_limit_count", PROPERTY_HINT_RANGE, "0,16384,1,or_greater"));
	for (int i = 0; i < direction_limits.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::VECTOR3, "direction_limits/" + itos(i) + "/control_point"));
		p_list->push_back(
				PropertyInfo(Variant::REAL, "direction_limits/" + itos(i) + "/radius"));
	}
}

void IKConstraintKusudama::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_twist_limit", "axial_limit"), &IKConstraintKusudama::set_twist_limit);
	ClassDB::bind_method(D_METHOD("get_twist_limit"), &IKConstraintKusudama::get_twist_limit);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "twist_limit", PROPERTY_HINT_RESOURCE_TYPE, "IKTwistLimit"), "set_twist_limit", "get_twist_limit");
}

Ref<IKTwistLimit> IKConstraintKusudama::get_twist_limit() const {
	return axial_limit;
}

void IKConstraintKusudama::set_twist_limit(Ref<IKTwistLimit> p_axial_limit) {
	set_axial_limits(axial_limit->get_min_twist_angle(), from_tau(axial_limit->get_range()));
	_change_notify();
	emit_changed();
}

void CMDDInverseKinematic::ChainTarget::set_parent_target(CMDDInverseKinematic::ChainTarget *parent) {
	if (parent_target != NULL) {
		parent_target->remove_child_target(this);
	}
	//set the parent to the global axes if the user
	//tries to set the pin to be its own parent

	if (parent != NULL) {
		parent->add_child_target(this);
		parent_target = parent;
	}
}

void CMDDInverseKinematic::ChainTarget::remove_child_target(CMDDInverseKinematic::ChainTarget *child) {
	int32_t target_i = child_targets.find(child);
	if (target_i != -1) {
		child_targets.remove(target_i);
	}
}

void CMDDInverseKinematic::ChainTarget::add_child_target(CMDDInverseKinematic::ChainTarget *newChild) {
	if (newChild->is_ancestor_of(this)) {
		set_parent_target(newChild->get_parent_target());
	}
	if (child_targets.find(newChild) != -1) {
		child_targets.push_back(newChild);
	}
}

CMDDInverseKinematic::ChainTarget *CMDDInverseKinematic::ChainTarget::get_parent_target() {
	return parent_target;
}

bool CMDDInverseKinematic::ChainTarget::is_ancestor_of(CMDDInverseKinematic::ChainTarget *potentialDescendent) {
	bool result = false;
	ChainTarget *cursor = potentialDescendent->get_parent_target();
	while (cursor) {
		if (cursor == this) {
			result = true;
			break;
		} else {
			cursor = cursor->parent_target;
		}
	}
	return result;
}

real_t CMDDInverseKinematic::ChainTarget::get_target_weight() {
	return target_weight;
}

void CMDDInverseKinematic::ChainItem::populate_return_dampening_iteration_array(Ref<IKConstraintKusudama> k) {
	float predampen = 1.0f - get_stiffness();
	float default_dampening = parent_armature->dampening;
	float dampening = parent_item == NULL ? Math_PI : predampen * default_dampening;
	float iterations = parent_armature->get_default_iterations();
	float returnful = k->get_pain();
	float falloff = 0.2f;
	half_returnful_dampened.resize(iterations);
	cos_half_returnful_dampened.resize(iterations);
	float iterations_pow = Math::pow(iterations, falloff * iterations * returnful);
	for (int32_t iter_i = 0; iter_i < iterations; iter_i++) {
		float iteration_scalar =
				((iterations_pow)-Math::pow(iter_i, falloff * iterations * returnful)) / (iterations_pow);
		float iteration_return_clamp = iteration_scalar * returnful * dampening;
		float cos_iteration_return_clamp = Math::cos(iteration_return_clamp / 2.0f);
		half_returnful_dampened.write()[iter_i] = iteration_return_clamp;
		cos_half_returnful_dampened.write()[iter_i] = cos_iteration_return_clamp;
	}
}

real_t CMDDInverseKinematic::ChainItem::get_bone_height() const {
	return bone_height;
}

void CMDDInverseKinematic::ChainItem::set_bone_height(const real_t p_bone_height) {
	bone_height = p_bone_height;
}
