/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#pragma once
class GraphNodeFormatNode;
class GraphNodeFormatPositionStrategy : public Reference {
	GDCLASS(GraphNodeFormatPositionStrategy, Reference);

protected:
	Rect2 total_bound;
	Vector<Vector<Ref<GraphNodeFormatNode> > > layered_nodes;

public:
	explicit GraphNodeFormatPositionStrategy(Vector<Vector<Ref<GraphNodeFormatNode> > > &r_in_layered_nodes) {
		layered_nodes = r_in_layered_nodes;
	}
	GraphNodeFormatPositionStrategy() {}
	virtual ~GraphNodeFormatPositionStrategy() {}
	Rect2 get_total_bound() const { return total_bound; }
};
