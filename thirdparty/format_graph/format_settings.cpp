/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "format_settings.h"

GraphNodeFormatSettings::PositioningAlgorithm GraphNodeFormatSettings::get_positioning_algorithm() const {
	return positioning_algorithm;
}

void GraphNodeFormatSettings::set_positioning_algorithm(GraphNodeFormatSettings::PositioningAlgorithm val) {
	positioning_algorithm = val;
}

int32_t GraphNodeFormatSettings::get_comment_border() const {
	return comment_border;
}

void GraphNodeFormatSettings::set_comment_border(int32_t val) {
	comment_border = val;
}

int32_t GraphNodeFormatSettings::get_horizontal_spacing() const {
	return horizontal_spacing;
}

void GraphNodeFormatSettings::set_horizontal_spacing(int32_t val) {
	horizontal_spacing = val;
}

int32_t GraphNodeFormatSettings::get_vertical_spacing() const {
	return vertical_spacing;
}

void GraphNodeFormatSettings::set_vertical_spacing(int32_t val) {
	vertical_spacing = val;
}

int32_t GraphNodeFormatSettings::get_max_ordering_repetitions() const {
	return max_ordering_repetitions;
}

void GraphNodeFormatSettings::set_max_ordering_repetitions(int32_t val) {
	max_ordering_repetitions = val;
}
