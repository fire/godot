/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "evenly_place_strategy.h"
#include "FormatterGraph.h"
#include "FormatterSettings.h"

Rect2 GraphNodeFormatEvenlyPlaceStrategy::PlaceNodeInLayer(Vector<Ref<GraphNodeFormatNode> > &Layer, const Rect2 &p_prebound) {
	Rect2 Bound;
	const GraphNodeFormatSettings settings; // TODO Use unique settings
	Vector2 position;
	if (p_prebound != Rect2()) {
		position = Vector2(p_prebound.Right + settings.get_horizontal_spacing(), 0);
	} else {
		position = Vector2(0, 0);
	}
	for (int32_t node_i = 0; node_i < Layer.size(); node_i++) {
		Ref<GraphNodeFormatNode> node = Layer[node_i];
		if (node->original_node == nullptr) {
			node->SetPosition(position);
			continue;
		}
		node->SetPosition(position);
		if (Bound != Rect2()) {
			Bound = Bound.expand(Rect2(position, node->Size));
		} else {
			Bound = Rect2(position, node->Size);
		}
		position.Y += node->Size.Y + settings.get_vertical_spacing();
	}
	return Bound;
}

Ref<GraphNodeFormatNode> GraphNodeFormatEvenlyPlaceStrategy::FindFirstNodeInLayeredList(Vector<Vector<Ref<GraphNodeFormatNode> > > &InLayeredNodes) {
	for (const auto &Layer : InLayeredNodes) {
		for (int32_t node_i = 0; node_i < Layer.size(); node_i++) {
			Ref<GraphNodeFormatNode> node = Layer[node_i];
			return node;
		}
	}
	return nullptr;
}

GraphNodeFormatEvenlyPlaceStrategy::GraphNodeFormatEvenlyPlaceStrategy(Vector<Vector<Ref<GraphNodeFormatNode> > > &InLayeredNodes) :
		IPositioningStrategy(InLayeredNodes) {
	Vector2 StartPosition;
	Ref<GraphNodeFormatNode> FirstNode = FindFirstNodeInLayeredList(InLayeredNodes);
	if (FirstNode != nullptr) {
		StartPosition = FirstNode->GetPosition();
	}

	float MaxHeight = 0;
	Rect2 PreBound;
	Vector<Rect2> Bounds;
	for (auto &Layer : InLayeredNodes) {
		PreBound = PlaceNodeInLayer(Layer, PreBound);
		Bounds.Add(PreBound);
		if (TotalBound.IsValid()) {
			TotalBound = TotalBound.Expand(PreBound);
		} else {
			TotalBound = PreBound;
		}
		const float Height = PreBound.GetSize().Y;
		if (Height > MaxHeight) {
			MaxHeight = Height;
		}
	}

	StartPosition -= Vector2(0, MaxHeight - Bounds[0].GetSize().Y) / 2.0f;

	for (int32 i = 0; i < InLayeredNodes.size(); i++) {
		const Vector2 Offset = Vector2(0, (MaxHeight - Bounds[i].GetSize().Y) / 2.0f) + StartPosition;
		for (auto Node : InLayeredNodes[i]) {
			Node->SetPosition(Node->GetPosition() + Offset);
		}
	}
	TotalBound = Rect2(StartPosition, TotalBound.GetSize());
}
