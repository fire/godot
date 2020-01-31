/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#pragma once

#include "core/math/rect2.h"
#include "position_strategy.h"

class GraphNodeFormatEvenlyPlaceStrategy : public GraphNodeFormatPositionStrategy {
	GDCLASS(GraphNodeFormatEvenlyPlaceStrategy, GraphNodeFormatPositionStrategy);

private:
	Rect2 place_node_in_layer(Vector<Ref<GraphNodeFormatNode> > &r_layer, const Rect2 &p_prebound);
	Ref<GraphNodeFormatNode> find_first_node_in_layered_list(Vector<Vector<Ref<GraphNodeFormatNode> > > &InLayeredNodes);

public:
	GraphNodeFormatEvenlyPlaceStrategy() {}
	explicit GraphNodeFormatEvenlyPlaceStrategy(Vector<Vector<Ref<GraphNodeFormatNode> > > &r_in_layered_nodes);
	virtual ~GraphNodeFormatEvenlyPlaceStrategy() {}
};
