/*************************************************************************/
/*  keyframe_reduce.h                                                    */
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

#ifndef KEYFRAME_REDUCE_H
#define KEYFRAME_REDUCE_H

#include "core/engine.h"

// Based on https://github.com/robertjoosten/maya-keyframe-reduction

// MIT License

// Copyright (c) 2019 Robert Joosten

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

class BezierKeyframeReduce : public Reference {
	GDCLASS(BezierKeyframeReduce, Reference);

public:
	class Vector2Bezier : public Vector2 {
	public:
		// Distance between two points
		// @return real_t distance
		real_t distance_between(Vector2Bezier p_other) const {
			return (*this - p_other).length();
		}
		// Signed angle between points
		// @return real_t
		real_t signed_angle(Vector2 p_this, Vector2 p_other) {
			real_t angle = p_this.angle_to(p_other);
			real_t cross = p_this.cross(p_other);
			if (cross < 0) {
				return -angle;
			}
			return angle;
		}
		Vector2Bezier() {
		}
		Vector2Bezier(real_t p_x, real_t p_y) {
			x = p_x;
			y = p_y;
		}
		Vector2Bezier(Vector2 p_vector2) {
			x = p_vector2.x;
			y = p_vector2.y;
		}
	};
	struct Bezier {
		Vector2Bezier in_handle; //relative (x always <0)
		Vector2Bezier out_handle; //relative (x always >0)
		Vector2Bezier time_value;
		bool weightedTangents = true;
		Bezier normalized() const {
			Bezier bezier = *this;
			bezier.in_handle = bezier.in_handle.normalized();
			bezier.out_handle = bezier.out_handle.normalized();
			bezier.time_value = bezier.time_value.normalized();
			return bezier;
		}
		Bezier operator-(Bezier p_other) const {
			Bezier bezier = *this;
			bezier.in_handle -= p_other.in_handle;
			bezier.out_handle -= p_other.out_handle;
			bezier.time_value -= p_other.time_value;
			return bezier;
		}
		Bezier() {
		}
		Bezier(Vector2Bezier p_time_value, Vector2Bezier p_in_handle, Vector2Bezier p_out_handle) {
			time_value = p_time_value;
			in_handle = p_in_handle;
			out_handle = p_out_handle;
		}
	};

	struct FitState {
		Vector<Bezier> points;
		real_t max_error;
		bool weighted_tangents;
	};

	const double EPSILON = 12e-11;
	const double THRESHOLD = 12e-5;

public:
	struct KeyframeReductionSetting {
		// Maximum allowed error when reducing the animation curves.
		real_t max_error = 0.1; // range(0, 1000) step(0.1)

		// Step size at which to sample the animation curves."
		real_t step_size = 1.0; // range(0.1, 100) step(1)

		// Created keyframes have weighted or non-weighted tangents.
		// Split tangents automatically, works on estimation.
		bool split_tangents_auto = true;
		bool weighted_tangents = false;
		bool tangent_split_existing = false;
		bool tangent_split_angle_threshold = false;
		// The threshold to split tangents.
		real_t tangent_split_angle_thresholdValue = 15.0f;
	};

private:
	// @param int/float p_start
	// @param int/float p_end
	// @param int/float p_step
	// @return Vector<String> range
	Vector<String> floatRange(double p_start, double p_end, double p_step) {
		Vector<String> values;

		// convert step to decimal to ensure float precision
		String step = rtos(p_step);
		while (p_start < p_end) {
			values.push_back(rtos(p_start));
			p_start += p_step;
		}

		return values;
	};

	// 	Add cubic bezier points between the provided first and last index
	// 	and it's tangents. Based in the weighted tangent settings the
	// 	iterations will be adjusted to gain speed. If a curve can be matched
	// 	the curve will be added to the keyframes, if not the curve will be
	// 	split at the point of max error and the function will be called
	// 	again.
	// 	@param int first
	// 	@param int last
	// 	@param Vector2Bezier tan1
	// 	@param Vector2Bezier tan2
	void fitCubic(const Vector<Bezier> &p_curves, Vector<Bezier> &r_keyframes, bool p_weighted_tangents, int32_t p_first, int32_t p_last, Vector2Bezier p_tan_1, Vector2Bezier p_tan_2, real_t p_error) {

		//use heuristic if region only has two points in it
		if (p_last - p_first == 1) {
			// get points
			Vector2Bezier pt1 = p_curves[p_first].time_value;
			Vector2Bezier pt2 = p_curves[p_last].time_value;

			// get distance between points
			real_t dist = pt1.distance_between(pt2) / 3;

			// add curve
			addCurve(r_keyframes, pt1, pt1 + p_tan_1 * dist, pt2 + p_tan_2 * dist, pt2);
			return;
		}

		// parameterize points, and attempt to fit curve
		Map<int, Vector2Bezier> uPrime = chordLengthParameterize(p_curves, p_first, p_last);
		real_t errorThreshold = MAX(p_error, p_error * 4);

		// get iterations, when weighted tangents is turned off the bezier
		// generator cannot be improved using multiple iterations.
		int32_t iterations = 1;
		if (p_weighted_tangents) {
			iterations = 4;
		}
		real_t maxError = 0;
		int32_t maxIndex = 0;
		for (int32_t i = 0; i < iterations; i++) {
			// generate curve
			Vector<Vector2Bezier> curve = generateBezier(p_curves, p_weighted_tangents, p_first, p_last, uPrime, p_tan_1, p_tan_2);

			// find max deviation of points to fitted curve
			Vector2Bezier max_error_vec = find_max_error(p_curves, p_first, p_last, curve, uPrime);
			maxError = max_error_vec.x;
			maxIndex = max_error_vec.y;

			// validate max error and add curve
			if (maxError < p_error) {
				addCurve(r_keyframes, curve[0], curve[1], curve[2], curve[3]);
				return;
			}

			// if error not too large, try reparameterization and iteration
			if (maxError >= errorThreshold) {
				break;
			}
			reparameterize(p_curves, p_first, p_last, uPrime, curve);
			errorThreshold = maxError;
		}

		// fitting failed -- split at max error point and fit recursively
		Bezier tanCenter = (p_curves[maxIndex - 1] - p_curves[maxIndex + 1]).normalized();

		fitCubic(p_curves, r_keyframes, p_weighted_tangents, p_first, maxIndex, p_tan_1, tanCenter.time_value, p_error);
		fitCubic(p_curves, r_keyframes, p_weighted_tangents, maxIndex, p_last, tanCenter.time_value * -1, p_tan_2, p_error);
	}

	// 	@param Vector2Bezier pt1:
	// 	@param Vector2Bezier tan1:
	// 	@param Vector2Bezier tan2:
	// 	@param Vector2Bezier pt2:
	void addCurve(Vector<Bezier> &r_curves, Vector2Bezier p_pt_1, Vector2Bezier p_tan_1, Vector2Bezier p_tan_2, Vector2Bezier p_pt_2) {
		// update previous keyframe with out handle
		Bezier &prev = r_curves.write[r_curves.size() - 1];
		prev.out_handle = p_tan_1 - p_pt_1;

		// create new keyframe
		Bezier keyframe(p_pt_2, Vector2Bezier(p_tan_2 - p_pt_2), Vector2Bezier());
		r_curves.push_back(keyframe);
	}

	// Based on the weighted tangent setting either use a least-squares
	// method to find Bezier controls points for a region or use Wu/Barsky
	// heuristic.
	// @param int first:
	// @param int last:
	// @param dict uPrime:
	// @param Vector2Bezier tan1:
	// @param Vector2Bezier tan2:
	Vector<Vector2Bezier> generateBezier(const Vector<Bezier> &p_curves, bool p_weighted_tangents, int32_t p_first, int32_t p_last, Map<int, Vector2Bezier> p_u_prime, Vector2Bezier p_tan_1, Vector2Bezier p_tan_2) {
		real_t epsilon = EPSILON;
		Vector2Bezier pt1 = p_curves[p_first].time_value;
		Vector2Bezier pt2 = p_curves[p_last].time_value;

		Vector2Bezier alpha1;
		Vector2Bezier alpha2;
		Vector2Bezier handle1;
		Vector2Bezier handle2;
		// use least-squares method to find Bezier control points for region.
		// Only if weighted tangents are allowed. If this is not the case we
		// will fall back on Wu/Barsky heuristic.
		if (p_weighted_tangents) {
			// create the C and X matrices
			Vector<Vector2Bezier> C;
			C.resize(4);
			Vector<Vector2Bezier> X;
			X.resize(2);
			int32_t range_i = p_last - p_first + 1;
			for (int32_t i = 0; i < range_i; i++) {
				Vector2Bezier u = p_u_prime[i];
				Vector2Bezier t = Vector2Bezier(1, 1) - u;
				Vector2Bezier b = Vector2Bezier(3, 3) * u * t;
				Vector2Bezier b0 = t * t * t;
				Vector2Bezier b1 = b * t;
				Vector2Bezier b2 = b * u;
				Vector2Bezier b3 = u * u * u;
				Vector2Bezier a1 = p_tan_1 * b1;
				Vector2Bezier a2 = p_tan_2 * b2;
				Vector2Bezier tmp = (p_curves[p_first + i].time_value - pt1 * (b0 + b1) - pt2 * (b2 + b3));
				C.write[0] += a1 * a1;
				C.write[1] += a1 * a2;
				C.write[2] = C[1];
				C.write[3] += a2 * a2;
				X.write[0] += a1 * tmp;
				X.write[1] += a2 * tmp;
			}

			//compute the determinants of C and X
			Vector2Bezier detC0C1 = C[0] * C[3] - C[2] * C[1];
			if (detC0C1.abs().x > Vector2(epsilon, epsilon).x && detC0C1.abs().y > Vector2(epsilon, epsilon).y) {
				//kramer's rule
				Vector2Bezier detC0X = C[0] * X[1] - C[2] * X[0];
				Vector2Bezier detXC1 = X[0] * C[3] - X[1] * C[1];

				//derive alpha values
				alpha1 = detXC1 / detC0C1;
				alpha2 = detC0X / detC0C1;
			} else {
				//matrix is under-determined, try assuming alpha1 == alpha2
				Vector2Bezier c0 = C[0] + C[1];
				Vector2Bezier c1 = C[2] + C[3];
				if (c0.abs().x > Vector2(epsilon, epsilon).x && c0.abs().y > Vector2(epsilon, epsilon).y) {
					alpha1 = alpha2 = X[0] / c0;
				} else if (c1.abs().x > Vector2(epsilon, epsilon).x && c1.abs().y > Vector2(epsilon, epsilon).y) {
					alpha1 = alpha2 = X[1] / c1;
				}
			}
		}

		// if alpha negative, use the Wu/Barsky heuristic (see text)
		// (if alpha is 0, you get coincident control points that lead to
		// divide by zero in any subsequent NewtonRaphsonRootFind() call.
		real_t segLength = pt2.distance_between(pt1);
		epsilon *= segLength;
		if ((alpha1.x < epsilon && alpha1.y < epsilon) && (alpha2.x < epsilon && alpha2.y < epsilon)) {
			// fall back on standard (probably inaccurate) formula,
			// and subdivide further if needed.
			alpha1 = alpha2 = Vector2Bezier(segLength / 3, segLength / 3);
		}
		// check if the found control points are in the right order when
		// projected onto the line through pt1 and pt2.
		Vector2Bezier line = pt2 - pt1;

		// control points 1 and 2 are positioned an alpha distance out
		// on the tangent vectors, left and right, respectively
		handle1 = p_tan_1 * alpha1;
		handle2 = p_tan_2 * alpha2;
		Vector2Bezier a = ((handle1 * line) - (handle2 * line));
		Vector2Bezier b = Vector2Bezier(segLength, segLength) * Vector2Bezier(segLength, segLength);
		if (a.x > b.x && a.y > b.y) {
			// fall back to the Wu/Barsky heuristic above.
			alpha1 = alpha2 = Vector2Bezier(segLength / 3, segLength / 3);
			handle1 = handle2 = Vector2Bezier();
		}

		// first and last control points of the Bezier curve are
		// positioned exactly at the first and last data points
		// Control points 1 and 2 are positioned an alpha distance out
		// on the tangent vectors, left and right, respectively
		Vector<Vector2Bezier> curve;
		curve.resize(4);
		curve.write[0] = pt1;
		Vector2Bezier pt2_tmp = p_tan_1 * alpha1;
		if (handle1 == Vector2Bezier()) {
			pt2_tmp = handle1;
		}
		curve.write[1] = pt1 + pt2_tmp;
		Vector2Bezier pt3_tmp = p_tan_2 * alpha2;
		if (handle2 == Vector2Bezier()) {
			pt2_tmp = handle2;
		}
		curve.write[2] = pt2 + pt3_tmp;
		curve.write[3] = pt2;
		return curve;
	}

	// Given set of points and their parameterization, try to find a better
	// parameterization.
	// @param int first:
	// @param int last:
	// @param dict u:
	// @param list curve:
	void reparameterize(Vector<Bezier> p_existing_curves, int32_t p_first, int32_t p_last, Map<int32_t, Vector2Bezier> &r_u, Vector<Vector2Bezier> p_curves) {
		int32_t i_range = p_last + 1;
		for (int32_t i = p_first; i < i_range; i++) {
			r_u[i - p_first] = findRoot(p_curves, p_existing_curves[i].time_value, r_u[i - p_first]);
		}
	}

	// Use Newton-Raphson iteration to find better root.
	// @param list curve:
	// @param Vector2Bezier point
	// @param Vector2Bezier u
	// @return New root point
	// @rtype Vector2Bezier
	Vector2Bezier findRoot(Vector<Vector2Bezier> p_curves, Vector2Bezier p_curve, Vector2Bezier p_u) {

		// generate control vertices for Q'
		Vector<Vector2Bezier> curve1;
		for (int32_t curve_i = 0; curve_i < 3; curve_i++) {
			curve1.push_back(p_curves[curve_i + 1] - p_curves[curve_i] * 3);
		}
		Vector<Vector2Bezier> curve2;
		for (int32_t curve_i = 0; curve_i < 2; curve_i++) {
			// generate control vertices for Q''
			curve2.push_back((curve1[curve_i + 1] - curve1[curve_i]) * 2);
		}

		// compute Q(u), Q'(u) and Q''(u)
		Vector2Bezier pt = evaluate(3, p_curves, p_u);
		Vector2Bezier pt1 = evaluate(2, curve1, p_u);
		Vector2Bezier pt2 = evaluate(1, curve2, p_u);
		Vector2Bezier diff = pt - p_curve;
		Vector2Bezier df = (pt1 * pt1) + (diff * pt2);

		// compute f(u) / f'(u)
		if (df.abs().x < EPSILON && df.abs().y < EPSILON) {
			return p_u;
		}
		// u = u - f(u) / f'(u)
		return p_u - (diff * pt1) / df;
	}

	// Evaluate a bezier curve at a particular parameter value.
	// @param int degree:
	// @param list curve:
	// @param float t:
	// @return  Vector2Bezier point on curve
	Vector2Bezier evaluate(int32_t p_degree, Vector<Vector2Bezier> p_curves, Vector2Bezier p_t) {

		// copy array
		Vector<Vector2Bezier> curves = p_curves;

		// triangle computation
		int32_t i_range = p_degree + 1;
		for (int32_t i = 1; i < i_range; i++) {
			int32_t j_range = p_degree - i + 1;
			for (int32_t j = 0; j < j_range; j++) {
				curves.write[j] = (curves[j] * (Vector2Bezier(1.0f, 1.0f) - p_t)) + (curves[j + 1] * p_t);
			}
		}

		return curves[0];
	}

	// Assign parameter values to digitized points using relative distances
	// between points.
	// @param int first:
	// @param int last:
	// @return dictionary of chord length parameterization
	Map<int, Vector2Bezier> chordLengthParameterize(Vector<Bezier> p_curves, int32_t p_first, int32_t p_last) {
		Map<int, Vector2Bezier> u;
		u.insert(0, Vector2Bezier());
		int32_t range_i = p_last + 1;
		for (int32_t i = p_first + 1; i < range_i; i++) {
			real_t value = p_curves[i].time_value.distance_between(
					p_curves[i - 1].time_value);
			u.insert(i - p_first, u[i - p_first - 1] + Vector2Bezier(value, value));
		}
		int32_t m = p_last - p_first;
		int32_t range_j = m + 1;
		for (int32_t j = 1; j < range_j; j++) {
			u[j] = u[j] / u[m];
		}
		return u;
	}

	// Find the maximum squared distance of digitized points to fitted
	// curve.
	// @param int first:
	// @param int last:
	// @param list curve:
	// @param dict u:
	// @return tuple of Max distance and max index
	Vector2Bezier find_max_error(const Vector<Bezier> &p_existing_curves, int32_t p_first, int32_t p_last, Vector<Vector2Bezier> p_curves, Map<int, Vector2Bezier> p_u) {

		real_t maxDist = 0.0f;
		real_t maxIndex = Math::floor((p_last - p_first + 1.0f) / 2.0f);

		for (int32_t i = p_first + 1; i < p_last; i++) {

			Vector2Bezier P = evaluate(3, p_curves, p_u[i - p_first]);
			real_t dist = (P - p_existing_curves[i].time_value).length();

			if (dist >= maxDist) {
				maxDist = dist;
				maxIndex = i;
			}
		}

		return Vector2Bezier(maxDist, maxIndex);
	}

	real_t min_real_list(Vector<real_t> p_reals) {
		if (p_reals.empty()) {
			return 0;
		}
		real_t min = p_reals[0];
		for (int32_t real_i = 0; real_i < p_reals.size(); real_i++) {
			min = MIN(min, p_reals[real_i]);
		}
		return min;
	}

	real_t max_real_list(Vector<real_t> p_reals) {
		if (p_reals.empty()) {
			return 0.0f;
		}
		real_t max = p_reals[0];
		for (int32_t real_i = 0; real_i < p_reals.size(); real_i++) {
			max = MAX(max, p_reals[real_i]);
		}
		return max;
	}

	real_t sum_real_list(Vector<real_t> p_reals) {
		if (p_reals.empty()) {
			return 0.0f;
		}
		real_t sum = 0.0f;
		for (int32_t real_i = 0; real_i < p_reals.size(); real_i++) {
			sum += p_reals[real_i];
		}
		return sum;
	}

	// The automatic tangent split will take the average of all values and
	// the average of just the minimum and maximum value and remaps that on
	// a logarithmic scale, this will give a predicted split angle value.
	// All angles will be processed to see if they fall in or outside that
	// threshold.
	// @param list of angles:
	// @return list of split indices
	Vector<int32_t> _findTangentSplitAuto(Vector<real_t> p_angles) {

		// get angles from points
		Vector<int32_t> splits;

		// get average variables
		real_t min_list = min_real_list(p_angles);
		real_t minAngle = min_list;
		if (min_list == 0) {
			minAngle = 0.00001;
		}
		real_t maxAngle = max_real_list(p_angles);
		real_t average = (minAngle + maxAngle) * 0.5;
		real_t mean = sum_real_list(p_angles) / p_angles.size() * 0.5;

		// get value at which to split
		real_t threshold = (Math::log(average) - Math::log(mean)) /
						   (Math::log(maxAngle) - Math::log(minAngle)) * average;

		if (mean * 10 > average) {
			return Vector<int32_t>();
		}

		// split based on angles
		for (int32_t angle_i = 0; angle_i < p_angles.size(); angle_i++) {
			if (p_angles[angle_i] > threshold) {
				splits.push_back(angle_i + 1);
			}
		}

		return splits;
	}

	// Loop existing frames and see if any keyframes contain tangents that
	// are not unified. If this is the case the index of the closest sampled
	// point will be returned.
	// @param list frames
	// @param int start
	// @param int end
	// @param int/float step
	// @return list of split indices
	Vector<int32_t> _find_tangent_split_existing(const Vector<Bezier> p_frames, int32_t p_start, int32_t p_end, real_t p_step) {
		Vector<int32_t> splits;
		for (int32_t i = 0; i < p_frames.size(); i++) {

			// get closest index
			int32_t index = int((i - p_start) / p_step);

			// validate split
			Vector2 frame_abs = (p_frames[i].in_handle - p_frames[i].out_handle).abs();
			if (frame_abs.x > THRESHOLD && frame_abs.y > THRESHOLD) {
				splits.push_back(index);
			}
		}

		return splits;
	}

	// The threshold tangent split will process all angles and check if that
	// angle falls in or outside of user provided threshold.
	// @param list angles:
	// @param int/float threshold:
	// @return list of split indices
	Vector<int32_t> _find_tangent_split_threshold(Vector<real_t> p_angles, real_t p_threshold) {

		Vector<int32_t> splits;
		// split based on angles
		for (int32_t i = 0; i < p_angles.size(); i++) {
			if (p_angles[i] > p_threshold) {
				splits.push_back(i + 1);
			}
		}
		return splits;
	}

	// Split provided points list based on the split indices provided. The
	// lists will have a duplicate end and start points relating to each
	// other.
	// @param list p_curves:
	// @param list p_split:
	// @return list of split bezier points
	Vector<Bezier> _split_points(const Vector<Bezier> &p_curves, Vector<int32_t> &p_split) {

		// validate split
		if (p_split.empty()) {
			return p_curves;
		}

		// complete split with adding start and end frames
		if (p_split[0] != 0) {
			p_split.insert(0, 0);
		}

		if (p_split[p_split.size() - 1 - 1] != p_curves.size()) {
			p_split.push_back(p_curves.size());
		}

		// make sure split is sorted and doesn't contain any duplicates
		Set<int32_t> split_set;
		for (int32_t split_i = 0; split_i < p_split.size(); split_i++) {
			split_set.insert(p_split[split_i]);
		}
		Vector<int32_t> split_list;
		for (int32_t split_i = 0; split_i < p_split.size(); split_i++) {
			split_list.push_back(p_split[split_i]);
		}
		p_split = split_list;
		p_split.sort();

		// split range for looping
		Vector<int32_t> splitA;
		for (int32_t i = 0; i < p_split.size() - 1; i++) {
			splitA.push_back(p_split[i]);
		}
		Vector<int32_t> splitB;
		for (int32_t i = 1; i < p_split.size(); i++) {
			splitB.push_back(p_split[i]);
		}

		// get lists
		Vector<Bezier> final_points;
		Vector<Vector2Bezier> zipped_splits;
		for (int32_t split_i = 0; split_i < splitA.size() && split_i < splitB.size(); split_i++) {
			zipped_splits.push_back(Vector2Bezier(splitA[split_i], splitB[split_i]));
		}
		for (int32_t zip_i = 0; zip_i < zipped_splits.size(); zip_i++) {
			Vector2Bezier zip = zipped_splits[zip_i];
			for (int32_t split_i = zip.x; split_i < zip.y; split_i++) {
				final_points.push_back(p_curves[split_i]);
			}
		}
		return final_points;
	}

	// Ported from Paper.js -
	// The Swiss Army Knife of Vector Graphics Scripting. http://paperjs.org/
	// Fit bezier curves to the points based on the provided maximum error
	// value and the bezier weighted tangents.
	// @return list of bezier segments
	Vector<Bezier> fit(FitState p_state) {
		real_t error = p_state.max_error;
		bool weightedTangent = p_state.weighted_tangents;
		int32_t length = p_state.points.size();
		if (length == 0) {
			return Vector<Bezier>();
		}
		Vector<Bezier> segments;
		// add first point as a keyframe
		segments.push_back(p_state.points[0]);

		// return segments if there is only 1 point
		if (length == 1) {
			return segments;
		}

		// get tangents
		Vector2Bezier tan1 = (p_state.points[1].time_value - p_state.points[0].time_value).normalized();
		Vector2Bezier tan2 = (p_state.points[length - 2].time_value - p_state.points[length - 1].time_value).normalized();

		// fit cubic
		fitCubic(p_state.points, segments, weightedTangent, 0, length - 1, tan1, tan2, error);

		return segments;
	}

	struct KeyframeTime {
		Vector<Vector2Bezier> points;
		Vector<real_t> angles;
	};

	Vector<real_t> get_values(Vector<Bezier> p_curves, Vector<String> p_frames) {
		Vector<real_t> values;
		for (int32_t frame_i = 0; frame_i < p_frames.size(); frame_i++) {
			int32_t frame = p_frames[frame_i].to_int();
			Vector2Bezier vec2 = p_curves[frame].time_value;
			real_t value = vec2.y;
			values.push_back(value);
		}
		return values;
	}

	// Sample the current animation curve based on the start and end frame,
	// and the provided step size. Vector2Ds and angles will be returned.
	// @param int start:
	// @param int end:
	// @param int/float step:
	// @return list of sample points and angles
	KeyframeTime sample(const Vector<Bezier> p_curves, int32_t p_start, int32_t p_end, real_t p_step) {
		KeyframeTime frame_values;
		// get frames and values
		Vector<String> frames = floatRange(p_start, p_end, p_step);
		Vector<real_t> values = get_values(p_curves, frames);

		// get points and angles
		Vector<real_t> angles;
		Vector<Vector2Bezier> points;
		for (int32_t frame_i = 0; frame_i < frames.size(); frame_i++) {
			real_t time = frames[frame_i].to_float();
			real_t value = values[frame_i];
			Vector2Bezier point = Vector2Bezier(time, value);
			points.push_back(point);
		}
		for (int32_t i = 1; i < points.size() - 2; i++) {
			Vector2Bezier v1 = points[i - 1] - points[i];
			Vector2Bezier v2 = points[i + 1] - points[i];
			real_t angle_to = v1.angle_to(v2);
			real_t rad = Math_PI - angle_to;
			real_t deg = Math::rad2deg(rad);
			angles.push_back(deg);
		}

		frame_values.points = points;
		frame_values.angles = angles;
		return frame_values;
	}

public:
	// Reduce the number of keyframes on the animation curve. Useful when
	// you are working with baked curves.
	// @param Vector<Bezier> r_points
	// @param Vector<Bezier> r_keyframes
	// @param KeyframeReductionSetting p_settings
	// @return float Reduction rate
	real_t reduce(const Vector<Bezier> &p_points, Vector<Bezier> &r_keyframes, KeyframeReductionSetting p_settings) {

		if (!p_points.size()) {
			return 0.0f;
		}
		Vector<Bezier> points = p_points;

		// get start and end frames
		int32_t start = 0;
		int32_t end = points.size() - 1;

		// get sample frames and values
		KeyframeTime sampled_frame_values = sample(points, start, end, p_settings.step_size);

		// get split indices
		Vector<int32_t> split;

		if (p_settings.split_tangents_auto) {
			split.append_array(_findTangentSplitAuto(sampled_frame_values.angles));
		}

		if (p_settings.tangent_split_existing) {
			split.append_array(_find_tangent_split_existing(
					p_points, start, end, p_settings.step_size));
		}
		if (p_settings.tangent_split_angle_threshold) {
			split.append_array(_find_tangent_split_threshold(
					sampled_frame_values.angles, p_settings.tangent_split_angle_thresholdValue));
		}
		{
			// get split points
			Vector<Bezier> split_points = _split_points(points, split);

			// fit points and get keyframes
			FitState state;
			state.max_error = p_settings.max_error;
			state.weighted_tangents = p_settings.weighted_tangents;
			state.points = split_points;
			r_keyframes = fit(state);
		}
		if (points.size() <= r_keyframes.size()) {
			r_keyframes = points;
			return 0;
		}

		real_t rate = r_keyframes.size() / float(points.size());
		return rate;
	}
};

#endif