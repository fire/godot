/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "format_graph.h"

bool GraphNodeFormatEdge::IsCrossing(const Ref<GraphNodeFormatEdge> Edge) const {
	return (From->IndexInLayer < Edge->From->IndexInLayer && To->IndexInLayer > Edge->To->IndexInLayer) || (From->IndexInLayer > Edge->From->IndexInLayer && To->IndexInLayer < Edge->To->IndexInLayer);
}

bool GraphNodeFormatEdge::IsInnerSegment() {
	return From->OwningNode->OriginalNode == nullptr && To->OwningNode->OriginalNode == nullptr;
}

GraphNodeFormatNode::GraphNodeFormatNode(const Ref<GraphNodeFormatNode> Other) :
		id(Other->id), OriginalNode(Other->OriginalNode), Size(Other->Size), PathDepth(Other->PathDepth), Position(Other->Position) {
	if (Other->SubGraph.is_valid()) {
		SubGraph = Other->SubGraph;
	}
	for (int32_t slot_i = 0; slot_i < Other->InSlots.size(); slot_i++) {
		Ref<GraphNodeFormatSlot> NewPin;
		NewPin.instance();
		Ref<GraphNodeFormatSlot> Pin = Other->InSlots[slot_i];
		NewPin->OriginalSlot = Pin->OriginalSlot;
		NewPin->Direction = Pin->Direction;
		NewPin->OwningNode = Ref<GraphNodeFormatNode>(this);
		NewPin->NodeOffset = Pin->NodeOffset;
		InSlots.push_back(NewPin);
	}
	for (int32_t slot_i = 0; slot_i < Other->OutSlots.size(); slot_i++) {
		Ref<GraphNodeFormatSlot> NewPin;
		NewPin.instance();
		Ref<GraphNodeFormatSlot> Pin = Other->InSlots[slot_i];
		NewPin->OriginalSlot = Pin->OriginalSlot;
		NewPin->Direction = Pin->Direction;
		NewPin->OwningNode = Ref<GraphNodeFormatNode>(this);
		NewPin->NodeOffset = Pin->NodeOffset;
		OutSlots.push_back(NewPin);
	}
}
GraphNodeFormatNode::GraphNodeFormatNode(const Ref<GraphNodeFormatNode> &Other) :
		id(InNode->get_path()), OriginalNode(InNode), SubGraph(nullptr), Size(Vector2()), PathDepth(0), Position(InNode->get_global_position()) {
	for (int32_t input_i = 0; input_i < InNode->get_connection_input_count(); input_i++) {
		Ref<GraphNodeFormatSlot> NewSlot;
		NewSlot.instance();
		NewSlot->OriginalSlot = input_i;
		NewSlot->Direction = GraphNodeFormatSlot::FORMAT_SLOT_INPUT;
		NewSlot->OwningNode = Ref<GraphNodeFormatNode>(this);
		InSlots.push_back(NewSlot);
	}
	for (int32_t output_i = 0; output_i < InNode->get_connection_output_count(); output_i++) {
		Ref<GraphNodeFormatSlot> NewSlot;
		NewSlot.instance();
		NewSlot->OriginalSlot = output_i;
		NewSlot->Direction = GraphNodeFormatSlot::FORMAT_SLOT_OUTPUT;
		NewSlot->OwningNode = Ref<GraphNodeFormatNode>(this);
		OutSlots.push_back(NewSlot);
	}
}

void GraphNodeFormatNode::Connect(GraphNodeFormatSlot SourcePin, GraphNodeFormatSlot TargetPin) {
	Ref<GraphNodeFormatEdge> Edge;
	Edge.instance();
	Edge->From = SourcePin;
	Edge->To = TargetPin;
	if (SourcePin->Direction == GraphNodeFormatSlot::FORMAT_SLOT_OUTPUT) {
		OutEdges.push_back(Edge);
	} else {
		InEdges.push_back(Edge);
	}
}

void GraphNodeFormatNode::Disconnect(GraphNodeFormatSlot SourcePin, GraphNodeFormatSlot TargetPin) {
	const auto Predicate = [SourcePin, TargetPin](const Ref<GraphNodeFormatEdge> Edge) {
		return Edge->From == SourcePin && Edge->To == TargetPin;
	};
	if (SourcePin->Direction == EGPD_Output) {
		const auto Index = OutEdges.IndexOfByPredicate(Predicate);
		if (Index != -1) {
			OutEdges.RemoveAt(Index);
		}
	} else {
		const auto Index = InEdges.IndexOfByPredicate(Predicate);
		if (Index != -1) {
			InEdges.RemoveAt(Index);
		}
	}
}

Vector<Ref<GraphNodeFormatNode> > GraphNodeFormatNode::GetSuccessors() const {
	Vector<Ref<GraphNodeFormatNode> > Result;
	for (auto Edge : OutEdges) {
		Result.Add(Edge->To->OwningNode);
	}
	return Result;
}

Vector<Ref<GraphNodeFormatNode> > GraphNodeFormatNode::GetPredecessors() const {
	Vector<Ref<GraphNodeFormatNode> > Result;
	for (auto Edge : InEdges) {
		Result.Add(Edge->To->OwningNode);
	}
	return Result;
}

bool GraphNodeFormatNode::IsSource() const {
	return InEdges.size() == 0;
}

bool GraphNodeFormatNode::IsSink() const {
	return OutEdges.size() == 0;
}

bool GraphNodeFormatNode::AnySuccessorPathDepthEqu0() const {
	for (auto OutEdge : OutEdges) {
		if (OutEdge->To->OwningNode->PathDepth == 0) {
			return true;
		}
	}
	return false;
}

float GraphNodeFormatNode::GetLinkedPositionToNode(const Ref<GraphNodeFormatNode> Node, int32_t Direction, bool IsHorizontalDirection) {
	auto &Edges = Direction == EGPD_Input ? InEdges : OutEdges;
	float MedianPosition = 0.0f;
	int32_t Count = 0;
	for (auto Edge : Edges) {
		if (Edge->To->OwningNode == Node) {
			if (IsHorizontalDirection) {
				MedianPosition += Edge->From->NodeOffset.Y;
			} else {
				MedianPosition += Edge->From->NodeOffset.X;
			}
			++Count;
		}
	}
	if (Count == 0) {
		return 0.0f;
	}
	return MedianPosition / Count;
}

bool GraphNodeFormatNode::IsCrossingInnerSegment(const Vector<Ref<GraphNodeFormatNode> > &LowerLayer, const Vector<Ref<GraphNodeFormatNode> > &UpperLayer) const {
	auto EdgesLinkedToUpper = GetEdgeLinkedToLayer(UpperLayer, EGPD_Input);
	auto EdgesBetweenTwoLayers = GraphNodeFormatGraph::GetEdgeBetweenTwoLayer(LowerLayer, UpperLayer, this);
	for (auto EdgeLinkedToUpper : EdgesLinkedToUpper) {
		for (auto EdgeBetweenTwoLayers : EdgesBetweenTwoLayers) {
			if (EdgeBetweenTwoLayers->IsInnerSegment() && EdgeLinkedToUpper->IsCrossing(EdgeBetweenTwoLayers)) {
				return true;
			}
		}
	}
	return false;
}

Ref<GraphNodeFormatNode> GraphNodeFormatNode::GetMedianUpper() const {
	Vector<Ref<GraphNodeFormatNode> > UpperNodes;
	for (auto InEdge : InEdges) {
		if (!UpperNodes.Contains(InEdge->To->OwningNode)) {
			UpperNodes.Add(InEdge->To->OwningNode);
		}
	}
	if (UpperNodes.size() > 0) {
		const int32_t m = UpperNodes.size() / 2;
		return UpperNodes[m];
	}
	return nullptr;
}

Ref<GraphNodeFormatNode> GraphNodeFormatNode::GetMedianLower() const {
	Vector<Ref<GraphNodeFormatNode> > LowerNodes;
	for (auto OutEdge : OutEdges) {
		if (!LowerNodes.Contains(OutEdge->To->OwningNode)) {
			LowerNodes.Add(OutEdge->To->OwningNode);
		}
	}
	if (LowerNodes.size() > 0) {
		const int32_t m = LowerNodes.size() / 2;
		return LowerNodes[m];
	}
	return nullptr;
}

Vector<Ref<GraphNodeFormatNode> > GraphNodeFormatNode::GetUppers() const {
	Set<Ref<GraphNodeFormatNode> > UpperNodes;
	for (auto InEdge : InEdges) {
		UpperNodes.Add(InEdge->To->OwningNode);
	}
	return UpperNodes.Array();
}

Vector<Ref<GraphNodeFormatNode> > GraphNodeFormatNode::GetLowers() const {
	Set<Ref<GraphNodeFormatNode> > LowerNodes;
	for (auto OutEdge : OutEdges) {
		LowerNodes.Add(OutEdge->To->OwningNode);
	}
	return LowerNodes.Array();
}

int32_t GraphNodeFormatNode::GetInputPinCount() const {
	return InSlots.size();
}

int32_t GraphNodeFormatNode::GetInputPinIndex(GraphNodeFormatSlot InputPin) const {
	return InSlots.Find(InputPin);
}

int32_t GraphNodeFormatNode::GetOutputPinCount() const {
	return OutSlots.size();
}

int32_t GraphNodeFormatNode::GetOutputPinIndex(GraphNodeFormatSlot OutputPin) const {
	return OutSlots.Find(OutputPin);
}

Vector<Ref<GraphNodeFormatEdge> > GraphNodeFormatNode::GetEdgeLinkedToLayer(const Vector<Ref<GraphNodeFormatNode> > &Layer, int32_t Direction) const {
	Vector<Ref<GraphNodeFormatEdge> > Result;
	const Vector<Ref<GraphNodeFormatEdge> > &Edges = Direction == EGPD_Output ? OutEdges : InEdges;
	for (auto Edge : Edges) {
		for (auto NextLayerNode : Layer) {
			if (Edge->To->OwningNode == NextLayerNode) {
				Result.Add(Edge);
			}
		}
	}
	return Result;
}

float GraphNodeFormatNode::CalcBarycenter(const Vector<Ref<GraphNodeFormatNode> > &Layer, int32_t Direction) const {
	auto Edges = GetEdgeLinkedToLayer(Layer, Direction);
	if (Edges.size() == 0) {
		return 0.0f;
	}
	float Sum = 0.0f;
	for (auto Edge : Edges) {
		Sum += Edge->To->IndexInLayer;
	}
	return Sum / Edges.size();
}

int32_t GraphNodeFormatNode::CalcPriority(int32_t Direction) const {
	if (OriginalNode == nullptr) {
		return 0;
	}
	return Direction == EGPD_Output ? OutEdges.size() : InEdges.size();
}

void GraphNodeFormatNode::InitPosition(Vector2 InPosition) {
	Position = InPosition;
}

void GraphNodeFormatNode::SetPosition(Vector2 InPosition) {
	const Vector2 Offset = InPosition - Position;
	Position = InPosition;
	if (SubGraph != nullptr) {
		SubGraph->OffsetBy(Offset);
	}
}

Vector2 GraphNodeFormatNode::GetPosition() const {
	return Position;
}

void GraphNodeFormatNode::SetSubGraph(Ref<GraphNodeFormatGraph> InSubGraph) {
	SubGraph = InSubGraph;
	auto SubGraphInPins = SubGraph->GetInputPins();
	auto SubGraphOutPins = SubGraph->GetOutputPins();
	for (auto Pin : SubGraphInPins) {
		auto NewPin = new FFormatterPin();
		NewPin->Guid = Pin->Guid;
		NewPin->OwningNode = this;
		NewPin->Direction = Pin->Direction;
		NewPin->NodeOffset = Pin->NodeOffset;
		NewPin->OriginalPin = Pin->OriginalPin;
		InSlots.Add(NewPin);
	}
	for (auto Pin : SubGraphOutPins) {
		auto NewPin = new FFormatterPin();
		NewPin->Guid = Pin->Guid;
		NewPin->OwningNode = this;
		NewPin->Direction = Pin->Direction;
		NewPin->NodeOffset = Pin->NodeOffset;
		NewPin->OriginalPin = Pin->OriginalPin;
		OutSlots.Add(NewPin);
	}
}

void GraphNodeFormatNode::UpdatePinsOffset() {
	if (SubGraph != nullptr) {
		auto PinsOffset = SubGraph->GetPinsOffset();
		for (auto Pin : InSlots) {
			if (PinsOffset.Contains(Pin->OriginalPin)) {
				Pin->NodeOffset = PinsOffset[Pin->OriginalPin];
			}
		}
		for (auto Pin : OutSlots) {
			if (PinsOffset.Contains(Pin->OriginalPin)) {
				Pin->NodeOffset = PinsOffset[Pin->OriginalPin];
			}
		}
		InSlots.Sort([](const FFormatterPin &A, const FFormatterPin &B) {
			return A.NodeOffset.Y < B.NodeOffset.Y;
		});
		OutSlots.Sort([](const FFormatterPin &A, const FFormatterPin &B) {
			return A.NodeOffset.Y < B.NodeOffset.Y;
		});
	}
}

Set<GraphNode *> GraphNodeFormatGraph::PickChildren(const GraphNode *InNode, Set<GraphNode *> SelectedNodes) {
	const UEdGraphNode_Comment *CommentNode = Cast<UEdGraphNode_Comment>(InNode);
	auto ObjectsUnderComment = CommentNode->GetNodesUnderComment();
	Set<GraphNode *> SubSelectedNodes;
	for (auto Object : ObjectsUnderComment) {
		GraphNode *Node = Cast<GraphNode>(Object);
		if (Node != nullptr && SelectedNodes.Contains(Node) && !PickedNodes.Contains(Node)) {
			SubSelectedNodes.Add(Node);
			PickedNodes.Add(Node);
		}
	}
	return SubSelectedNodes;
}

Vector<Set<GraphNode *> > GraphNodeFormatGraph::FindIsolated(GraphEdit *InGraph, const Set<GraphNode *> &SelectedNodes) {
	Vector<Set<GraphNode *> > Result;
	Set<Ref<GraphNodeFormatNode> > CheckedNodes;
	Vector<Ref<GraphNodeFormatNode> > Stack;
	auto TempGraph = GraphNodeFormatGraph(InGraph, SelectedNodes, FFormatterDelegates(), true);
	for (auto Node : TempGraph.Nodes) {
		if (!CheckedNodes.Contains(Node)) {
			CheckedNodes.Add(Node);
			Stack.Push(Node);
		}
		Set<GraphNode *> IsolatedNodes;
		while (Stack.size() != 0) {
			Ref<GraphNodeFormatNode> Top = Stack.Pop();
			IsolatedNodes.Add(Top->OriginalNode);
			if (Top->SubGraph != nullptr) {
				IsolatedNodes.Append(Top->SubGraph->GetOriginalNodes());
			}
			Vector<Ref<GraphNodeFormatNode> > ConnectedNodes = Top->GetSuccessors();
			Vector<Ref<GraphNodeFormatNode> > Predecessors = Top->GetPredecessors();
			ConnectedNodes.Append(Predecessors);
			for (auto ConnectedNode : ConnectedNodes) {
				if (!CheckedNodes.Contains(ConnectedNode)) {
					Stack.Push(ConnectedNode);
					CheckedNodes.Add(ConnectedNode);
				}
			}
		}
		if (IsolatedNodes.size() != 0) {
			Result.Add(IsolatedNodes);
		}
	}
	return Result;
}

void GraphNodeFormatGraph::CalculateNodesSize(FuncRef SizeCalculator) {
	if (IsolatedGraphs.size() > 1) {
		for (auto IsolatedGraph : IsolatedGraphs) {
			IsolatedGraph->CalculateNodesSize(SizeCalculator);
		}
	} else {
		for (auto Node : Nodes) {
			if (Node->OriginalNode != nullptr) {
				if (SubGraphs.Contains(Node->Guid)) {
					SubGraphs[Node->Guid]->CalculateNodesSize(SizeCalculator);
				}
				Variant::CallError call_error;
				Node->Size = SizeCalculator.call_func(&Node->OriginalNode, 1, &call_error);
				ERR_FAIL_COND(call_error.error != OK);
			}
		}
	}
}

void GraphNodeFormatGraph::CalculatePinsOffset(FOffsetCalculatorDelegate OffsetCalculator) {
	if (IsolatedGraphs.size() > 1) {
		for (auto IsolatedGraph : IsolatedGraphs) {
			IsolatedGraph->CalculatePinsOffset(OffsetCalculator);
		}
	} else {
		for (auto Node : Nodes) {
			if (Node->OriginalNode != nullptr) {
				if (SubGraphs.Contains(Node->Guid)) {
					SubGraphs[Node->Guid]->CalculatePinsOffset(OffsetCalculator);
				}
				for (auto Pin : Node->InPins) {
					Pin->NodeOffset = OffsetCalculator.Execute(Pin->OriginalPin);
				}
				for (auto Pin : Node->OutPins) {
					Pin->NodeOffset = OffsetCalculator.Execute(Pin->OriginalPin);
				}
			}
		}
	}
}

Vector<GraphNode *> GraphNodeFormatGraph::GetSortedCommentNodes(GraphEdit *InGraph, Set<GraphNode *> SelectedNodes) {
	Vector<GraphNode *> CommentNodes;
	for (int child_i = 0; child_i < InGraph->get_child_count(); child_i++) {
		Node *node = InGraph->get_child(child_i);
		GraphNode *graph_node = Object::cast_to<GraphNode>(node);
		if (!graph_node) {
			continue;
		}
		if (!SelectedNodes.has(graph_node)) {
			continue;
		}
		if (graph_node->is_comment()) {
			CommentNodes.push_back(graph_node);
		}
	}
	return CommentNodes;
}

Vector<GraphNodeFormatEdge> GraphNodeFormatGraph::GetEdgeForNode(Ref<GraphNodeFormatNode> Node, Set<GraphNode *> SelectedNodes) {
	Vector<GraphNodeFormatEdge> Result;
	auto OriginalNode = Node->OriginalNode;
	if (SubGraphs.Contains(OriginalNode->get_path())) {
		const Set<GraphNode *> InnerSelectedNodes = SubGraphs[OriginalNode->NodePath]->GetOriginalNodes();
		for (auto SelectedNode : InnerSelectedNodes) {
			for (auto Pin : SelectedNode->Pins) {
				for (auto LinkedToPin : Pin->LinkedTo) {
					const auto LinkedToNode = LinkedToPin->GetOwningNodeUnchecked();
					if (InnerSelectedNodes.Contains(LinkedToNode) || !SelectedNodes.Contains(LinkedToNode)) {
						continue;
					}
					GraphNodeFormatSlot From = OriginalPinsMap[Pin];
					GraphNodeFormatSlot To = OriginalPinsMap[LinkedToPin];
					Result.Add(GraphNodeFormatEdge{ From, To });
				}
			}
		}
	} else {
		for (auto Pin : OriginalNode->Pins) {
			for (auto LinkedToPin : Pin->LinkedTo) {
				const auto LinkedToNode = LinkedToPin->GetOwningNodeUnchecked();
				if (!SelectedNodes.Contains(LinkedToNode)) {
					continue;
				}
				GraphNodeFormatSlot From = OriginalPinsMap[Pin];
				GraphNodeFormatSlot To = OriginalPinsMap[LinkedToPin];
				Result.Add(GraphNodeFormatEdge{ From, To });
			}
		}
	}
	return Result;
}

Vector<Ref<GraphNodeFormatNode> > GraphNodeFormatGraph::GetSuccessorsForNodes(Set<Ref<GraphNodeFormatNode> > Nodes) {
	Vector<Ref<GraphNodeFormatNode> > Result;
	for (auto Node : Nodes) {
		for (auto outEdge : Node->OutEdges) {
			if (!Nodes.Contains(outEdge->To->OwningNode)) {
				Result.Add(outEdge->To->OwningNode);
			}
		}
	}
	return Result;
}

Vector<Ref<GraphNodeFormatNode> > GraphNodeFormatGraph::GetNodesGreaterThan(int32_t i, Set<Ref<GraphNodeFormatNode> > &Excluded) {
	Vector<Ref<GraphNodeFormatNode> > Result;
	for (auto Node : Nodes) {
		if (!Excluded.Contains(Node) && Node->PathDepth >= i) {
			Result.Add(Node);
		}
	}
	return Result;
}

void GraphNodeFormatGraph::BuildNodes(GraphEdit *InGraph, Set<GraphNode *> SelectedNodes) {
	//Vector<UEdGraphNode_Comment*> SortedCommentNodes = GetSortedCommentNodes(InGraph, SelectedNodes);
	//for (int32_t i = SortedCommentNodes.size() - 1; i != -1; --i)
	//{
	//	UEdGraphNode_Comment* CommentNode = SortedCommentNodes[i];
	//	if (PickedNodes.Contains(CommentNode))
	//	{
	//		continue;
	//	}
	//	FormatNode* NodeData = CollapseNode(CommentNode, SelectedNodes);
	//	AddNode(NodeData);
	//	PickedNodes.Add(CommentNode);
	//}
	for (auto Node : InGraph->Nodes) {
		if (!SelectedNodes.Contains(Node) || PickedNodes.Contains(Node)) {
			continue;
		}
		Ref<GraphNodeFormatNode> NodeData = new GraphNodeFormatNode(Node);
		AddNode(NodeData);
		PickedNodes.Add(Node);
	}
}

void GraphNodeFormatGraph::BuildEdges(Set<GraphNode *> SelectedNodes) {
	for (auto Node : Nodes) {
		auto Edges = GetEdgeForNode(Node, SelectedNodes);
		for (auto Edge : Edges) {
			Node->Connect(Edge.From, Edge.To);
		}
	}
}

void GraphNodeFormatGraph::BuildNodesAndEdges(GraphEdit *InGraph, Set<GraphNode *> SelectedNodes) {
	BuildNodes(InGraph, SelectedNodes);
	BuildEdges(SelectedNodes);
	Nodes.Sort([](const Ref<GraphNodeFormatNode> A, const Ref<GraphNodeFormatNode> B) {
		return A.GetPosition().Y < B.GetPosition().Y;
	});
}

void GraphNodeFormatGraph::DoLayering() {
	LayeredList.Empty();
	Set<Ref<GraphNodeFormatNode> > Set;
	for (int32_t i = CalculateLongestPath(); i != 0; i--) {
		Set<Ref<GraphNodeFormatNode> > Layer;
		auto Successors = GetSuccessorsForNodes(Set);
		auto NodesToProcess = GetNodesGreaterThan(i, Set);
		NodesToProcess.Append(Successors);
		for (auto Node : NodesToProcess) {
			auto Predecessors = Node->GetPredecessors();
			bool bPredecessorsFinished = true;
			for (auto Predecessor : Predecessors) {
				if (!Set.Contains(Predecessor)) {
					bPredecessorsFinished = false;
					break;
				}
			}
			if (bPredecessorsFinished) {
				Layer.Add(Node);
			}
		}
		Set.Append(Layer);
		Vector<Ref<GraphNodeFormatNode> > Array = Layer.Array();
		if (funcrefs.NodeComparer.IsBound()) {
			Array.Sort([this](const Ref<GraphNodeFormatNode> A, const Ref<GraphNodeFormatNode> B) {
				return funcrefs.NodeComparer.Execute(A, B);
			});
		}
		LayeredList.Add(Array);
	}
}

void GraphNodeFormatGraph::RemoveCycle() {
	auto ClonedGraph = new GraphNodeFormatGraph(*this);
	while (auto SourceNode = ClonedGraph->FindSourceNode()) {
		ClonedGraph->RemoveNode(SourceNode);
	}
	while (auto SinkNode = ClonedGraph->FindSinkNode()) {
		ClonedGraph->RemoveNode(SinkNode);
	}
	while (auto MedianNode = ClonedGraph->FindMedianNode()) {
		for (auto Edge : MedianNode->InEdges) {
			GraphNodeFormatSlot From = PinsMap[Edge->From->Guid];
			GraphNodeFormatSlot To = PinsMap[Edge->To->Guid];
			NodesMap[MedianNode->id]->Disconnect(From, To);
			To->OwningNode->Disconnect(To, From);
		}
		ClonedGraph->RemoveNode(MedianNode);
	}
	delete ClonedGraph;
}

void GraphNodeFormatGraph::AddDummyNodes() {
	for (int i = 0; i < LayeredList.size() - 1; i++) {
		auto &Layer = LayeredList[i];
		auto &NextLayer = LayeredList[i + 1];
		for (auto Node : Layer) {
			Vector<Ref<GraphNodeFormatEdge> > LongEdges;
			for (auto Edge : Node->OutEdges) {
				if (!NextLayer.Contains(Edge->To->OwningNode)) {
					LongEdges.Add(Edge);
				}
			}
			for (auto Edge : LongEdges) {
				Node->Disconnect(Edge->From, Edge->To);
				auto dummyNode = new GraphNodeFormatNode();
				AddNode(dummyNode);
				Node->Connect(Edge->From, dummyNode->InSlots[0]);
				dummyNode->Connect(dummyNode->InSlots[0], Edge->From);
				dummyNode->Connect(dummyNode->OutSlots[0], Edge->To);
				Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
				Edge->To->OwningNode->Connect(Edge->To, dummyNode->OutSlots[0]);
				NextLayer.Add(dummyNode);
			}
		}
	}
}

void GraphNodeFormatGraph::SortInLayer(Vector<Vector<Ref<GraphNodeFormatNode> > > &Order, int32_t Direction) {
	if (Order.size() < 2) {
		return;
	}
	const int Start = Direction == EGPD_Output ? Order.size() - 2 : 1;
	const int End = Direction == EGPD_Output ? -1 : Order.size();
	const int Step = Direction == EGPD_Output ? -1 : 1;
	for (int i = Start; i != End; i += Step) {
		auto &FixedLayer = Order[i - Step];
		auto &FreeLayer = Order[i];
		for (Ref<GraphNodeFormatNode> Node : FreeLayer) {
			Node->OrderValue = Node->CalcBarycenter(FixedLayer, Direction);
		}
		FreeLayer.StableSort([](const Ref<GraphNodeFormatNode> A, const Ref<GraphNodeFormatNode> B) -> bool {
			return A.OrderValue < B.OrderValue;
		});
		CalculatePinsIndexInLayer(FreeLayer);
	}
}

void GraphNodeFormatGraph::DoOrderingSweep() {
	const UFormatterSettings *Settings = GetDefault<UFormatterSettings>();
	auto Best = LayeredList;
	auto Order = LayeredList;
	int32_t BestCrossing = INT_MAX;
	for (int i = 0; i < Settings->MaxOrderingIterations; i++) {
		SortInLayer(Order, i % 2 == 0 ? EGPD_Input : EGPD_Output);
		const int32_t NewCrossing = CalculateCrossing(Order);
		if (NewCrossing < BestCrossing) {
			Best = Order;
			BestCrossing = NewCrossing;
		}
	}
	LayeredList = Best;
}

void GraphNodeFormatGraph::DoPositioning() {
	const UFormatterSettings &Settings = *GetDefault<UFormatterSettings>();

	if (funcrefs.IsVerticalPositioning.IsBound() && funcrefs.IsVerticalPositioning.Execute()) {
		FFastAndSimplePositioningStrategy FastAndSimplePositioningStrategy(LayeredList, false);
		TotalBound = FastAndSimplePositioningStrategy.GetTotalBound();
		return;
	}

	if (Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EEvenlyInLayer) {
		FEvenlyPlaceStrategy LeftToRightPositioningStrategy(LayeredList);
		TotalBound = LeftToRightPositioningStrategy.GetTotalBound();
	} else if (Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian || Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodTop) {
		FFastAndSimplePositioningStrategy FastAndSimplePositioningStrategy(LayeredList);
		TotalBound = FastAndSimplePositioningStrategy.GetTotalBound();
	} else if (Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::ELayerSweep) {
		FPriorityPositioningStrategy PriorityPositioningStrategy(LayeredList);
		TotalBound = PriorityPositioningStrategy.GetTotalBound();
	}
}

GraphNodeFormatNode GraphNodeFormatGraph::CollapseNode(GraphNode *InNode, Set<GraphNode *> SelectedNodes) {
	Ref<GraphNodeFormatNode> Node = new GraphNodeFormatNode(InNode);
	Ref<GraphNodeFormatGraph> SubGraph = BuildSubGraph(InNode, SelectedNodes);
	if (SubGraph != nullptr) {
		Node->SetSubGraph(SubGraph);
	}
	return Node;
}

GraphNodeFormatGraph GraphNodeFormatGraph::BuildSubGraph(const GraphNode *InNode, Set<GraphNode *> SelectedNodes) {
	Set<GraphNode *> SubSelectedNodes = PickChildren(InNode, SelectedNodes);
	if (SubSelectedNodes.size() > 0) {
		Ref<GraphNodeFormatGraph> SubGraph = new GraphNodeFormatGraph(graph_edit, SubSelectedNodes, funcrefs);
		return SubGraph;
	}
	return nullptr;
}

GraphNodeFormatNode GraphNodeFormatGraph::FindSourceNode() const {
	for (auto Node : Nodes) {
		if (Node->IsSource()) {
			return Node;
		}
	}
	return nullptr;
}

GraphNodeFormatNode GraphNodeFormatGraph::FindSinkNode() const {
	for (auto Node : Nodes) {
		if (Node->IsSink()) {
			return Node;
		}
	}
	return nullptr;
}

GraphNodeFormatNode GraphNodeFormatGraph::FindMedianNode() const {
	Ref<GraphNodeFormatNode> Result = nullptr;
	int32_t MaxDegreeDiff = 0;
	for (auto Node : Nodes) {
		const int32_t DegreeDiff = Node->OutEdges.size() - Node->InEdges.size();
		if (DegreeDiff >= MaxDegreeDiff) {
			MaxDegreeDiff = DegreeDiff;
			Result = Node;
		}
	}
	return Result;
}

Vector<Ref<GraphNodeFormatNode> > GraphNodeFormatGraph::GetLeavesWidthPathDepthEqu0() const {
	Vector<Ref<GraphNodeFormatNode> > Result;
	for (auto Node : Nodes) {
		if (Node->PathDepth != 0 || Node->AnySuccessorPathDepthEqu0()) {
			continue;
		}
		Result.Add(Node);
	}
	return Result;
}

int32_t GraphNodeFormatGraph::CalculateLongestPath() const {
	int32_t LongestPath = 1;
	while (true) {
		auto Leaves = GetLeavesWidthPathDepthEqu0();
		if (Leaves.size() == 0) {
			break;
		}
		for (auto leaf : Leaves) {
			leaf->PathDepth = LongestPath;
		}
		LongestPath++;
	}
	LongestPath--;
	return LongestPath;
}

Set<GraphNode *> GraphNodeFormatGraph::GetChildren(const GraphNode *InNode) const {
	const UEdGraphNode_Comment *CommentNode = Cast<UEdGraphNode_Comment>(InNode);
	auto ObjectsUnderComment = CommentNode->GetNodesUnderComment();
	Set<GraphNode *> SubSelectedNodes;
	for (auto Object : ObjectsUnderComment) {
		GraphNode *Node = Cast<GraphNode>(Object);
		if (Node != nullptr) {
			SubSelectedNodes.Add(Node);
		}
	}
	return SubSelectedNodes;
}

GraphNodeFormatGraph::GraphNodeFormatGraph(const GraphNodeFormatGraph &Other) {
	graph_edit = Other->graph_edit;
	funcrefs = Other->funcrefs;
	for (auto Node : Other->Nodes) {
		Ref<GraphNodeFormatNode> Cloned = new GraphNodeFormatNode(*Node);
		AddNode(Cloned);
	}
	for (auto Node : Other->Nodes) {
		for (auto Edge : Node->InEdges) {
			GraphNodeFormatSlot From = PinsMap[Edge->From->id];
			GraphNodeFormatSlot To = PinsMap[Edge->To->id];
			NodesMap[Node->Guid]->Connect(From, To);
		}
		for (auto Edge : Node->OutEdges) {
			GraphNodeFormatSlot From = PinsMap[Edge->From->id];
			GraphNodeFormatSlot To = PinsMap[Edge->To->id];
			NodesMap[Node->Guid]->Connect(From, To);
		}
	}
	for (auto Isolated : Other->IsolatedGraphs) {
		IsolatedGraphs.Add(new GraphNodeFormatGraph(*Isolated));
	}
}

GraphNodeFormatGraph::GraphNodeFormatGraph(GraphEdit *InGraph, const Set<GraphNode *> &SelectedNodes, FFormatterDelegates InDelegates, bool IsSingleMode) {
	graph_edit = InGraph;
	funcrefs = InDelegates;
	if (IsSingleMode) {
		BuildNodesAndEdges(InGraph, SelectedNodes);
	} else {
		auto FoundIsolatedGraphs = FindIsolated(InGraph, SelectedNodes);
		if (FoundIsolatedGraphs.size() > 1) {
			for (const auto &IsolatedNodes : FoundIsolatedGraphs) {
				auto NewGraph = new GraphNodeFormatGraph(InGraph, IsolatedNodes, InDelegates);
				IsolatedGraphs.Add(NewGraph);
			}
		} else if (FoundIsolatedGraphs.size() == 1) {
			BuildNodesAndEdges(InGraph, FoundIsolatedGraphs[0]);
		}
	}
}

GraphNodeFormatGraph::~GraphNodeFormatGraph() {
	for (auto Node : Nodes) {
		delete Node;
	}
	for (auto Graph : IsolatedGraphs) {
		delete Graph;
	}
}

void GraphNodeFormatGraph::AddNode(Ref<GraphNodeFormatNode> InNode) {
	Nodes.Add(InNode);
	NodesMap.Add(InNode->id, InNode);
	if (InNode->SubGraph != nullptr) {
		SubGraphs.Add(InNode->id, InNode->SubGraph);
	}
	for (auto Pin : InNode->InSlots) {
		if (Pin->OriginalPin != nullptr) {
			OriginalPinsMap.Add(Pin->OriginalPin, Pin);
		}
		PinsMap.Add(Pin->Guid, Pin);
	}
	for (auto Pin : InNode->OutSlots) {
		if (Pin->OriginalPin != nullptr) {
			OriginalPinsMap.Add(Pin->OriginalPin, Pin);
		}
		PinsMap.Add(Pin->Guid, Pin);
	}
}

void GraphNodeFormatGraph::RemoveNode(Ref<GraphNodeFormatNode> NodeToRemove) {
	Vector<Ref<GraphNodeFormatEdge> > Edges = NodeToRemove->InEdges;
	for (auto Edge : Edges) {
		Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
	}
	Edges = NodeToRemove->OutEdges;
	for (auto Edge : Edges) {
		Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
	}
	Nodes.Remove(NodeToRemove);
	NodesMap.Remove(NodeToRemove->id);
	SubGraphs.Remove(NodeToRemove->id);
	for (auto Pin : NodeToRemove->InSlots) {
		OriginalPinsMap.Remove(Pin->OriginalPin);
		PinsMap.Remove(Pin->Guid);
	}
	for (auto Pin : NodeToRemove->OutSlots) {
		OriginalPinsMap.Remove(Pin->OriginalPin);
		PinsMap.Remove(Pin->Guid);
	}
	delete NodeToRemove;
}

void GraphNodeFormatGraph::Format() {
	const UFormatterSettings &Settings = *GetDefault<UFormatterSettings>();
	if (IsolatedGraphs.size() > 1) {
		Rect2 PreBound;
		for (auto isolatedGraph : IsolatedGraphs) {
			isolatedGraph->Format();
			if (PreBound.IsValid()) {
				isolatedGraph->SetPosition(PreBound.GetBottomLeft());
			}
			auto Bound = isolatedGraph->GetTotalBound();
			if (TotalBound.IsValid()) {
				TotalBound = TotalBound.Expand(Bound);
			} else {
				TotalBound = Bound;
			}
			PreBound = TotalBound.OffsetBy(Vector2(0, Settings.VerticalSpacing));
		}
	} else {
		CalculateNodesSize(FuncRef BoundCalculator);
		CalculatePinsOffset(FuncRef OffsetCalculator);
		for (auto SubGraphPair : SubGraphs) {
			auto SubGraph = SubGraphPair.Value;
			auto Node = NodesMap[SubGraphPair.Key];
			SubGraph->Format();
			Node->UpdatePinsOffset();
			auto Bound = SubGraph->GetTotalBound();
			Node->InitPosition(Bound.GetTopLeft() - Vector2(Settings.CommentBorder, Settings.CommentBorder));
			Node->Size = SubGraph->GetTotalBound().GetSize() + Vector2(Settings.CommentBorder * 2, Settings.CommentBorder * 2);
		}
		if (Nodes.size() > 0) {
			RemoveCycle();
			DoLayering();
			AddDummyNodes();
			if (!funcrefs.NodeComparer.IsBound()) {
				DoOrderingSweep();
			}
			DoPositioning();
		}
	}
}

Rect2 GraphNodeFormatGraph::GetTotalBound() const {
	return TotalBound;
}

Map<GraphNode *, Rect2> GraphNodeFormatGraph::GetBoundMap() {
	Map<GraphNode *, Rect2> Result;
	if (IsolatedGraphs.size() > 0) {
		for (auto Graph : IsolatedGraphs) {
			Result.Append(Graph->GetBoundMap());
		}
		return Result;
	}
	for (auto Node : Nodes) {
		if (Node->OriginalNode == nullptr) {
			continue;
		}
		Result.Add(Node->OriginalNode, Rect2::FromPointAndExtent(Node->GetPosition(), Node->Size));
		if (SubGraphs.Contains(Node->Guid)) {
			Result.Append(SubGraphs[Node->Guid]->GetBoundMap());
		}
	}
	return Result;
}

void GraphNodeFormatGraph::SetPosition(const Vector2 &Position) {
	const Vector2 Offset = Position - TotalBound.GetTopLeft();
	OffsetBy(Offset);
}

void GraphNodeFormatGraph::OffsetBy(const Vector2 &InOffset) {
	if (IsolatedGraphs.size() > 0) {
		for (auto isolatedGraph : IsolatedGraphs) {
			isolatedGraph->OffsetBy(InOffset);
		}
	} else {
		for (auto Node : Nodes) {
			Node->SetPosition(Node->GetPosition() + InOffset);
		}
	}
	TotalBound = TotalBound.OffsetBy(InOffset);
}

Map<GraphPort *, Vector2> GraphNodeFormatGraph::GetPinsOffset() {
	Map<UEdGraphPin *, Vector2> Result;
	const UFormatterSettings &Settings = *GetDefault<UFormatterSettings>();
	const auto Border = Vector2(Settings.CommentBorder, Settings.CommentBorder);
	if (IsolatedGraphs.size() > 0) {
		for (auto IsolatedGraph : IsolatedGraphs) {
			auto SubBound = IsolatedGraph->GetTotalBound();
			auto Offset = SubBound.GetTopLeft() - TotalBound.GetTopLeft();
			auto SubOffsets = IsolatedGraph->GetPinsOffset();
			for (auto SubOffsetPair : SubOffsets) {
				SubOffsetPair.Value = SubOffsetPair.Value + Offset;
			}
			Result.Append(SubOffsets);
		}
		return Result;
	}
	for (auto Node : Nodes) {
		for (auto OutPin : Node->OutPins) {
			Vector2 PinOffset = Node->Position + OutPin->NodeOffset - TotalBound.GetTopLeft() + Border;
			Result.Add(OutPin->OriginalPin, PinOffset);
		}
		for (auto InPin : Node->InPins) {
			Vector2 PinOffset = Node->Position + InPin->NodeOffset - TotalBound.GetTopLeft() + Border;
			Result.Add(InPin->OriginalPin, PinOffset);
		}
	}
	return Result;
}

Vector<Ref<GraphNodeFormatSlot> > GraphNodeFormatGraph::GetInputPins() const {
	Set<GraphNodeFormatSlot> Result;
	if (IsolatedGraphs.size() > 0) {
		for (auto IsolatedGraph : IsolatedGraphs) {
			Result.Append(IsolatedGraph->GetInputPins());
		}
		return Result.Array();
	}
	for (auto Node : Nodes) {
		for (auto Pin : Node->InPins) {
			Result.Add(Pin);
		}
	}
	return Result.Array();
}

Vector<Ref<GraphNodeFormatSlot> > GraphNodeFormatGraph::GetOutputPins() const {
	Set<GraphNodeFormatSlot> Result;
	if (IsolatedGraphs.size() > 0) {
		for (auto IsolatedGraph : IsolatedGraphs) {
			Result.Append(IsolatedGraph->GetOutputPins());
		}
		return Result.Array();
	}
	for (auto Node : Nodes) {
		for (auto Pin : Node->OutPins) {
			Result.Add(Pin);
		}
	}
	return Result.Array();
}

Set<GraphNode *> GraphNodeFormatGraph::GetOriginalNodes() const {
	Set<GraphNode *> Result;
	if (IsolatedGraphs.size() > 0) {
		for (auto IsolatedGraph : IsolatedGraphs) {
			Result.Append(IsolatedGraph->GetOriginalNodes());
		}
		return Result;
	}
	for (auto Node : Nodes) {
		if (SubGraphs.Contains(Node->Guid)) {
			Result.Append(SubGraphs[Node->Guid]->GetOriginalNodes());
		}
		if (Node->OriginalNode != nullptr) {
			Result.Add(Node->OriginalNode);
		}
	}
	return Result;
}

Array GraphNodeFormatGraph::GetEdgeBetweenTwoLayer(const Vector<Ref<GraphNodeFormatNode> > &LowerLayer, const Vector<Ref<GraphNodeFormatNode> > &UpperLayer, const Ref<GraphNodeFormatNode> ExcludedNode /*= nullptr*/) {
	Array Result;
	for (int32_t layer_i = 0; layer_i < LowerLayer; layer_i++) {
		if (ExcludedNode == LowerLayer[layer_i]) {
			continue;
		}
		Result.push_back(Node->GetEdgeLinkedToLayer(UpperLayer, GraphNodeFormatSlot::FORMAT_SLOT_INPUT));
	}
	return Result;
}

Vector<Rect2> GraphNodeFormatGraph::CalculateLayersBound(Vector<Vector<Ref<GraphNodeFormatNode> > > &InLayeredNodes, bool IsHorizontalDirection /*= true*/) {
	Vector<Rect2> LayersBound;
	const UFormatterSettings &Settings = *GetDefault<UFormatterSettings>();
	Rect2 TotalBound;
	Vector2 Spacing;
	if (IsHorizontalDirection) {
		Spacing = Vector2(Settings.HorizontalSpacing, 0);
	} else {
		Spacing = Vector2(0, Settings.VerticalSpacing);
	}
	for (int32_t i = 0; i < InLayeredNodes.size(); i++) {
		const auto &Layer = InLayeredNodes[i];
		Rect2 Bound;
		Vector2 Position;
		if (TotalBound.IsValid()) {
			Position = TotalBound.GetBottomRight() + Spacing;
		} else {
			Position = Vector2(0, 0);
		}
		for (auto Node : Layer) {
			if (Bound.IsValid()) {
				Bound = Bound.Expand(Rect2::FromPointAndExtent(Position, Node->Size));
			} else {
				Bound = Rect2::FromPointAndExtent(Position, Node->Size);
			}
		}
		LayersBound.Add(Bound);
		if (TotalBound.IsValid()) {
			TotalBound = TotalBound.Expand(Bound);
		} else {
			TotalBound = Bound;
		}
	}
	return LayersBound;
}

void GraphNodeFormatGraph::CalculatePinsIndex(const Vector<Vector<Ref<GraphNodeFormatNode> > > &Order) {
	for (int32_t i = 0; i < Order.size(); i++) {
		auto &Layer = Order[i];
		CalculatePinsIndexInLayer(Layer);
	}
}

void GraphNodeFormatGraph::CalculatePinsIndexInLayer(const Vector<Ref<GraphNodeFormatNode> > &Layer) {
	int32_t InPinStartIndex = 0, OutPinStartIndex = 0;
	for (int32_t j = 0; j < Layer.size(); j++) {
		for (auto InPin : Layer[j]->InSlots) {
			InPin->IndexInLayer = InPinStartIndex + Layer[j]->GetInputPinIndex(InPin);
		}
		for (auto OutPin : Layer[j]->OutSlots) {
			OutPin->IndexInLayer = OutPinStartIndex + Layer[j]->GetOutputPinIndex(OutPin);
		}
		OutPinStartIndex += Layer[j]->GetOutputPinCount();
		InPinStartIndex += Layer[j]->GetInputPinCount();
	}
}
