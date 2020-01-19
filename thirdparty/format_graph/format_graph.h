/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once
#include "core/func_ref.h"
#include "core/list.h"
#include "core/math/rect2.h"
#include "core/math/vector2.h"
#include "core/node_path.h"
#include "core/reference.h"
#include "core/set.h"
#include "format_funcrefs.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/graph_node.h"
#include <stdint.h>

class GraphNodeFormatGraph;
class GraphNodeFormatNode;
class GraphNodeFormatEdge;

using GraphPort = int;
using GraphPortDirection = int;

class GraphNodeFormatSlot : public Reference {
	GDCLASS(GraphNodeFormatSlot, Reference);

public:
	enum {
		FORMAT_SLOT_INPUT,
		FORMAT_SLOT_OUTPUT
	};
	GraphPort OriginalSlot{ -1 };
	GraphPortDirection Direction{ FORMAT_SLOT_INPUT };
	Ref<GraphNodeFormatNode> OwningNode;
	Vector2 NodeOffset;
	int32_t IndexInLayer{ -1 };
};

class GraphNodeFormatEdge : public Reference {
	GDCLASS(GraphNodeFormatEdge, Reference);

public:
	Ref<GraphNodeFormatSlot> From;
	Ref<GraphNodeFormatSlot> To;
	bool IsCrossing(const Ref<GraphNodeFormatEdge> Edge) const;

	bool IsInnerSegment();
};

class GraphNodeFormatNode : public Reference {
	GDCLASS(GraphNodeFormatNode, Reference);

public:
	NodePath id;
	GraphNode *OriginalNode;
	Ref<GraphNodeFormatGraph> SubGraph;
	Vector2 Size;
	Vector<Ref<GraphNodeFormatEdge> > InEdges;
	Vector<Ref<GraphNodeFormatEdge> > OutEdges;
	Vector<Ref<GraphNodeFormatSlot> > InSlots;
	Vector<Ref<GraphNodeFormatSlot> > OutSlots;
	int32_t PathDepth;
	int32_t PositioningPriority;
	GraphNodeFormatNode(Ref<GraphNode> InNode);
	GraphNodeFormatNode(const Ref<GraphNodeFormatNode> &Other);
	~GraphNodeFormatNode() {}
	GraphNodeFormatNode() {}
	void Connect(GraphNodeFormatSlot SourcePin, GraphNodeFormatSlot TargetPin);
	void Disconnect(GraphNodeFormatSlot SourcePin, GraphNodeFormatSlot TargetPin);
	Vector<GraphNodeFormatNode *> GetSuccessors() const;
	Vector<GraphNodeFormatNode *> GetPredecessors() const;
	bool IsSource() const;
	bool IsSink() const;
	bool AnySuccessorPathDepthEqu0() const;
	float GetLinkedPositionToNode(const GraphNodeFormatNode *Node, int32_t Direction, bool IsHorizontalDirection);
	bool IsCrossingInnerSegment(const Vector<GraphNodeFormatNode *> &LowerLayer, const Vector<GraphNodeFormatNode *> &UpperLayer) const;
	GraphNodeFormatNode *GetMedianUpper() const;
	GraphNodeFormatNode *GetMedianLower() const;
	Vector<GraphNodeFormatNode *> GetUppers() const;
	Vector<GraphNodeFormatNode *> GetLowers() const;
	int32_t GetInputPinCount() const;
	int32_t GetInputPinIndex(GraphNodeFormatSlot InputPin) const;
	int32_t GetOutputPinCount() const;
	int32_t GetOutputPinIndex(GraphNodeFormatSlot OutputPin) const;
	Vector<Ref<GraphNodeFormatEdge> > GetEdgeLinkedToLayer(const Vector<GraphNodeFormatNode *> &Layer, int32_t Direction) const;
	float CalcBarycenter(const Vector<GraphNodeFormatNode *> &Layer, int32_t Direction) const;
	int32_t CalcPriority(int32_t Direction) const;
	void InitPosition(Vector2 InPosition);
	void SetPosition(Vector2 InPosition);
	Vector2 GetPosition() const;
	void SetSubGraph(Ref<GraphNodeFormatGraph> InSubGraph);
	void UpdatePinsOffset();
	friend class GraphNodeFormatGraph;

private:
	float OrderValue;
	Vector2 Position;
};

class GraphNodeFormatGraph : public Reference {
	GDCLASS(GraphNodeFormatGraph, Reference);

private:
	static Vector<Set<GraphNode *> > FindIsolated(GraphEdit *InGraph, const Set<GraphNode *> &SelectedNodes);
	void CalculateNodesSize(FuncRef SizeCalculator);
	void CalculatePinsOffset(FuncRef OffsetCalculator);
	Vector<GraphNode *> GetSortedCommentNodes(GraphEdit *InGraph, Set<GraphNode *> SelectedNodes);
	Vector<GraphNodeFormatEdge> GetEdgeForNode(GraphNodeFormatNode *Node, Set<GraphNode *> SelectedNodes);
	static Vector<GraphNodeFormatNode *> GetSuccessorsForNodes(Set<GraphNodeFormatNode *> Nodes);
	Vector<GraphNodeFormatNode *> GetNodesGreaterThan(int32_t i, Set<GraphNodeFormatNode *> &Excluded);
	void BuildNodes(GraphEdit *InGraph, Set<GraphNode *> SelectedNodes);
	void BuildEdges(Set<GraphNode *> SelectedNodes);
	void BuildNodesAndEdges(GraphEdit *InGraph, Set<GraphNode *> SelectedNodes);
	void DoLayering();
	void RemoveCycle();
	void AddDummyNodes();
	void SortInLayer(Vector<Vector<GraphNodeFormatNode *> > &Order, int32_t Direction);
	void DoOrderingSweep();
	void DoPositioning();
	GraphNodeFormatNode CollapseNode(GraphNode *InNode, Set<GraphNode *> SelectedNodes);
	GraphNodeFormatGraph BuildSubGraph(const GraphNode *InNode, Set<GraphNode *> SelectedNodes);
	GraphNodeFormatNode FindSourceNode() const;
	GraphNodeFormatNode FindSinkNode() const;
	GraphNodeFormatNode FindMedianNode() const;
	Vector<GraphNodeFormatNode *> GetLeavesWidthPathDepthEqu0() const;
	int32_t CalculateLongestPath() const;
	Set<GraphNode *> GetChildren(const GraphNode *InNode) const;
	Set<GraphNode *> PickChildren(const GraphNode *InNode, Set<GraphNode *> SelectedNodes);

	GraphEdit *graph_edit;
	FormatFuncrefs funcrefs;
	Vector<Ref<GraphNodeFormatNode> > Nodes;
	Map<NodePath, Ref<GraphNodeFormatNode> > NodesMap;
	Map<NodePath, Ref<GraphNodeFormatGraph> > SubGraphs;
	Map<NodePath, Ref<GraphNodeFormatSlot> > PinsMap;
	Map<GraphPort, Ref<GraphNodeFormatSlot> > OriginalPinsMap;
	Set<GraphNode *> PickedNodes;
	Vector<Vector<Ref<GraphNodeFormatNode> > > LayeredList;
	Vector<Ref<GraphNodeFormatGraph> > IsolatedGraphs;
	Rect2 TotalBound;

public:
	GraphNodeFormatGraph(GraphEdit *InGraph, const Set<GraphNode *> &SelectedNodes, FormatFuncrefs InDelegates, bool IsSingleMode);
	GraphNodeFormatGraph(const GraphNodeFormatGraph &Other);
	GraphNodeFormatGraph() {}
	~GraphNodeFormatGraph() {}

	void AddNode(GraphNodeFormatNode *InNode);
	void RemoveNode(GraphNodeFormatNode *NodeToRemove);
	void Format();
	Rect2 GetTotalBound() const;
	Map<GraphNode *, Rect2> GetBoundMap();
	void SetPosition(const Vector2 &Position);
	void OffsetBy(const Vector2 &InOffset);
	Map<GraphPort *, Vector2> GetPinsOffset();
	Vector<Ref<GraphNodeFormatSlot> > GetInputPins() const;
	Vector<Ref<GraphNodeFormatSlot> > GetOutputPins() const;
	Set<GraphNode *> GetOriginalNodes() const;

	/**
	 * Get edges between two layers.
	 */

	static Array GetEdgeBetweenTwoLayer(const Vector<GraphNodeFormatNode *> &LowerLayer, const Vector<GraphNodeFormatNode *> &UpperLayer, const GraphNodeFormatNode *ExcludedNode = nullptr);

	static Vector<Rect2> CalculateLayersBound(Vector<Vector<GraphNodeFormatNode *> > &InLayeredNodes, bool IsHorizontalDirection = true);

	static void CalculatePinsIndex(const Vector<Vector<GraphNodeFormatNode *> > &Order);

	static void CalculatePinsIndexInLayer(const Vector<GraphNodeFormatNode *> &Layer);
};

inline static int32_t CalculateCrossing(const Vector<Vector<GraphNodeFormatNode *> > &Order) {
	GraphNodeFormatGraph::CalculatePinsIndex(Order);
	int32_t CrossingValue = 0;
	for (int i = 1; i < Order.size(); i++) {
		const auto &UpperLayer = Order[i - 1];
		const auto &LowerLayer = Order[i];
		Array NodeEdges = GraphNodeFormatGraph::GetEdgeBetweenTwoLayer(LowerLayer, UpperLayer);
		while (NodeEdges.size() != 0) {
			Ref<GraphNodeFormatEdge> Edge1 = NodeEdges.pop_back();
			for (int32_t edge_i = 0; edge_i < NodeEdges.size(); edge_i++) {
				if (Edge1->IsCrossing(NodeEdges[edge_i])) {
					CrossingValue++;
				}
			}
		}
	}
	return CrossingValue;
}
