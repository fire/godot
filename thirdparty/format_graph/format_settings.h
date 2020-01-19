/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef FORMATTER_SETTINGS_H
#define FORMATTER_SETTINGS_H

#include "core/engine.h"
class GraphNodeFormatSettings : public Reference {
	GDCLASS(GraphNodeFormatSettings, Reference);

public:
	enum PositioningAlgorithm {
		GRAPH_FORMAT_ALGORITHM_POSITION_EVENLY_IN_LAYER,
		GRAPH_FORMAT_ALGORITHM_FAST_AND_SIMPLE_METHOD_TOP,
		GRAPH_FORMAT_ALGORITHM_FAST_AND_SIMPLE_METHOD_MEDIAN,
		GRAPH_FORMAT_ALGORITHM_LAYER_SWEEP
	};

	GraphNodeFormatSettings() {
		set_positioning_algorithm(GraphNodeFormatSettings::GRAPH_FORMAT_ALGORITHM_FAST_AND_SIMPLE_METHOD_TOP);
		set_comment_border(45);
		set_horizontal_spacing(100);
		set_vertical_spacing(80);
		set_max_ordering_repetitions(10);
	}

	~GraphNodeFormatSettings() {
	}

	/** Positioning algorithm*/
	PositioningAlgorithm get_positioning_algorithm() const;
	void set_positioning_algorithm(PositioningAlgorithm val);
	/** Border thickness */
	int32_t get_comment_border() const;
	void set_comment_border(int32_t val);
	/** Spacing between two layers */
	int32_t get_horizontal_spacing() const;
	void set_horizontal_spacing(int32_t val);
	/** Spacing between two nodes */
	int32_t get_vertical_spacing() const;
	void set_vertical_spacing(int32_t val);
	/** Vertex ordering max iterations */
	int32_t get_max_ordering_repetitions() const;
	void set_max_ordering_repetitions(int32_t val);

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_positioning_algorithm", "positioning_algorithm"), &GraphNodeFormatSettings::set_positioning_algorithm);
		ClassDB::bind_method(D_METHOD("get_positioning_algorithm"), &GraphNodeFormatSettings::get_positioning_algorithm);
		ClassDB::bind_method(D_METHOD("set_comment_border", "comment_border"), &GraphNodeFormatSettings::set_comment_border);
		ClassDB::bind_method(D_METHOD("get_comment_border"), &GraphNodeFormatSettings::get_comment_border);
		ClassDB::bind_method(D_METHOD("set_horizontal_spacing", "horizontal_spacing"), &GraphNodeFormatSettings::set_horizontal_spacing);
		ClassDB::bind_method(D_METHOD("get_horizontal_spacing"), &GraphNodeFormatSettings::get_horizontal_spacing);
		ClassDB::bind_method(D_METHOD("set_max_ordering_repetitions", "max_ordering_iterations"), &GraphNodeFormatSettings::set_max_ordering_repetitions);
		ClassDB::bind_method(D_METHOD("get_max_ordering_repetitions"), &GraphNodeFormatSettings::get_max_ordering_repetitions);
		ClassDB::bind_method(D_METHOD("set_vertical_spacing", "vertical_spacing"), &GraphNodeFormatSettings::set_vertical_spacing);
		ClassDB::bind_method(D_METHOD("get_vertical_spacing"), &GraphNodeFormatSettings::get_vertical_spacing);

		BIND_CONSTANT(GraphNodeFormatSettings::GRAPH_FORMAT_ALGORITHM_POSITION_EVENLY_IN_LAYER);
		BIND_CONSTANT(GraphNodeFormatSettings::GRAPH_FORMAT_ALGORITHM_FAST_AND_SIMPLE_METHOD_TOP);
		BIND_CONSTANT(GraphNodeFormatSettings::GRAPH_FORMAT_ALGORITHM_FAST_AND_SIMPLE_METHOD_MEDIAN);
		BIND_CONSTANT(GraphNodeFormatSettings::GRAPH_FORMAT_ALGORITHM_LAYER_SWEEP);

		ADD_PROPERTY(PropertyInfo(Variant::INT, "positioning_algorithm", PROPERTY_HINT_ENUM, "Position Evenly In Layer,Fast and Simple Method Top,Fast and Simple Method Median,Layer Sweep"), "set_positioning_algorithm", "get_positioning_algorithm");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "comment_border", PROPERTY_HINT_RANGE, "45,,1"), "set_comment_border", "get_comment_border");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "horizontal_spacing", PROPERTY_HINT_RANGE, "0,,1"), "set_horizontal_spacing", "get_horizontal_spacing");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "max_ordering_repetitions", PROPERTY_HINT_RANGE, "0,100,1"), "set_max_ordering_repetitions", "get_max_ordering_repetitions");
		ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_spacing", PROPERTY_HINT_RANGE, "0,,1"), "set_vertical_spacing", "get_vertical_spacing");
	}

private:
	PositioningAlgorithm positioning_algorithm;
	int32_t comment_border;
	int32_t horizontal_spacing;
	int32_t vertical_spacing;
	int32_t max_ordering_repetitions;
};
VARIANT_ENUM_CAST(GraphNodeFormatSettings::PositioningAlgorithm);
#endif // FORMATTER_SETTINGS_H
