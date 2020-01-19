/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterSettings.h"

GraphNodeFormatSettings::PositioningAlgorithm GraphNodeFormatSettings::get_positioning_algorithm() const {
	return positioning_algorithm;
}

void GraphNodeFormatSettings::set_positioning_algorithm(GraphNodeFormatSettings::PositioningAlgorithm val) {
	positioning_algorithm = val;
}

int32_t GraphNodeFormatSettings::get_comment_border() const {
	return CommentBorder;
}

void GraphNodeFormatSettings::set_comment_border(int32_t val) {
	CommentBorder = val;
}

int32_t GraphNodeFormatSettings::get_horizontal_spacing() const {
	return HorizontalSpacing;
}

void GraphNodeFormatSettings::set_horizontal_spacing(int32_t val) {
	HorizontalSpacing = val;
}

int32_t GraphNodeFormatSettings::get_vertical_spacing() const {
	return VerticalSpacing;
}

void GraphNodeFormatSettings::set_vertical_spacing(int32_t val) {
	VerticalSpacing = val;
}

int32_t GraphNodeFormatSettings::get_max_ordering_iterations() const {
	return MaxOrderingIterations;
}

void GraphNodeFormatSettings::set_max_ordering_iterations(int32_t val) {
	MaxOrderingIterations = val;
}
