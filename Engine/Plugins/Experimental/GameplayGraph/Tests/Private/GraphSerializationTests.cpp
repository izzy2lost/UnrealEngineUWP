// Copyright Epic Games, Inc. All Rights Reserved.
#include "TestHarness.h"
#include "TestGraphBuilder.h"

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Write", "[graph]")
{
	PopulateVertices(6, true);
	BuildLinearEdges(3);

	CHECK(Graph->NumVertices() == 6);
	CHECK(Graph->NumEdges() == 4);
	CHECK(Graph->NumIslands() == 2);

	FSerializableGraph SerializedGraph = Graph->GetSerializableGraph();
	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(SerializedGraph.Vertices.Num() == 6);
	CHECK(SerializedGraph.Vertices.Contains(VertexHandles[0]) == true);
	CHECK(SerializedGraph.Vertices.Contains(VertexHandles[1]) == true);
	CHECK(SerializedGraph.Vertices.Contains(VertexHandles[2]) == true);
	CHECK(SerializedGraph.Vertices.Contains(VertexHandles[3]) == true);
	CHECK(SerializedGraph.Vertices.Contains(VertexHandles[4]) == true);
	CHECK(SerializedGraph.Vertices.Contains(VertexHandles[5]) == true);

	CHECK(SerializedGraph.Edges.Num() == 4);
	for (const FGraphVertexHandle& VertexHandle : VertexHandles)
	{
		UGraphVertex* Vertex = VertexHandle.GetVertex();
		REQUIRE(Vertex != nullptr);

		Vertex->ForEachAdjacentVertex(
			[&SerializedGraph](const FGraphVertexHandle& NeighborVertexHandle, const FGraphEdgeHandle& EdgeHandle)
			{
				REQUIRE(SerializedGraph.Edges.Contains(EdgeHandle) == true);
				const FSerializedEdgeData& EdgeData = SerializedGraph.Edges[EdgeHandle];
				UGraphEdge* Edge = EdgeHandle.GetEdge();
				REQUIRE(Edge != nullptr);
				CHECK(EdgeData.Node1 == Edge->NodeA());
				CHECK(EdgeData.Node2 == Edge->NodeB());
			}
		);
	}

	CHECK(SerializedGraph.Islands.Num() == 2);
	CHECK(SerializedGraph.Islands.Contains(IslandHandles[0]) == true);
	CHECK(SerializedGraph.Islands.Contains(IslandHandles[1]) == true);
	for (const FGraphIslandHandle& IslandHandle : IslandHandles)
	{
		UGraphIsland* Island = IslandHandle.GetIsland();
		REQUIRE(Island != nullptr);

		REQUIRE(SerializedGraph.Islands.Contains(IslandHandle) == true);
		const FSerializedIslandData& IslandData = SerializedGraph.Islands[IslandHandle];

		CHECK(IslandData.Vertices.Num() == Island->Num());
		for (const FGraphVertexHandle& VertexHandle : Island->GetVertices())
		{
			CHECK(IslandData.Vertices.Contains(VertexHandle) == true);
		}
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Read (normal)", "[graph]")
{
	FSerializableGraph SerializedGraph;
	SerializedGraph.Properties.bGenerateIslands = true;

	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });

	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[0], SerializedGraph.Vertices[1] });
	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[1], SerializedGraph.Vertices[2] });
	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[3], SerializedGraph.Vertices[4] });
	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[4], SerializedGraph.Vertices[5] });

	SerializedGraph.Islands.Add(FGraphIslandHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedIslandData { TArray{SerializedGraph.Vertices[0], SerializedGraph.Vertices[1], SerializedGraph.Vertices[2]} });
	SerializedGraph.Islands.Add(FGraphIslandHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedIslandData { TArray{SerializedGraph.Vertices[3], SerializedGraph.Vertices[4], SerializedGraph.Vertices[5]} });
	Graph->LoadFromSerializedGraph(SerializedGraph);

	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(Graph->NumVertices() == 6);
	CHECK(Graph->NumEdges() == 4);
	CHECK(Graph->NumIslands() == 2);

	int32 Index = 0;
	for (const FGraphVertexHandle& SerializedVertexHandle : SerializedGraph.Vertices)
	{
		CHECK(Graph->GetVertices().Contains(SerializedVertexHandle) == true);

		UGraphVertex* LoadedVertex = Graph->GetVertices().FindRef(SerializedVertexHandle);
		REQUIRE(LoadedVertex != nullptr);
		CHECK(SerializedVertexHandle == LoadedVertex->Handle());
	}

	for (const TPair<FGraphEdgeHandle, FSerializedEdgeData>& Edge: SerializedGraph.Edges)
	{
		CHECK(Graph->GetEdges().Contains(Edge.Key) == true);

		CHECK(Graph->GetVertices().Contains(Edge.Value.Node1) == true);
		CHECK(Graph->GetVertices().Contains(Edge.Value.Node2) == true);

		UGraphEdge* LoadedEdge = Graph->GetEdges().FindRef(Edge.Key);
		REQUIRE(LoadedEdge != nullptr);
		CHECK(LoadedEdge->ContainsNode(Edge.Value.Node1) == true);
		CHECK(LoadedEdge->NodeA().IsComplete() == true);
		CHECK(LoadedEdge->ContainsNode(Edge.Value.Node2) == true);
		CHECK(LoadedEdge->NodeB().IsComplete() == true);
	}

	for (const TPair<FGraphIslandHandle, FSerializedIslandData>& Island : SerializedGraph.Islands)
	{
		CHECK(Graph->GetIslands().Contains(Island.Key) == true);

		UGraphIsland* LoadedIsland = Graph->GetIslands().FindRef(Island.Key);
		REQUIRE(LoadedIsland != nullptr);

		for (const FGraphVertexHandle& IslandVertexHandle : Island.Value.Vertices)
		{
			CHECK(LoadedIsland->GetVertices().Contains(IslandVertexHandle) == true);

			const FGraphVertexHandle* LoadedIslandVertexHandle = LoadedIsland->GetVertices().Find(IslandVertexHandle);
			REQUIRE(LoadedIslandVertexHandle != nullptr);
			CHECK(LoadedIslandVertexHandle->IsComplete() == true);
			CHECK(LoadedIslandVertexHandle->GetGraph() == Graph);
		}
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Read::Errors::Vertex::Invalid Index", "[graph]")
{
	FSerializableGraph SerializedGraph;
	SerializedGraph.Properties.bGenerateIslands = true;
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex{ FGuid{0, 0, 0, 0} }, nullptr });

	Graph->LoadFromSerializedGraph(SerializedGraph);
	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(Graph->NumVertices() == 0);
	CHECK(Graph->NumEdges() == 0);
	CHECK(Graph->NumIslands() == 0);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Read::Errors::Edge::Invalid Index", "[graph]")
{
	FSerializableGraph SerializedGraph;
	SerializedGraph.Properties.bGenerateIslands = true;
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex{FGuid{0, 0, 0, 0}}, nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[0], SerializedGraph.Vertices[1] });

	Graph->LoadFromSerializedGraph(SerializedGraph);
	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(Graph->NumVertices() == 2);
	CHECK(Graph->NumEdges() == 0);
	CHECK(Graph->NumIslands() == 0);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Read::Errors::Edge::Invalid Vertex Index", "[graph]")
{
	FSerializableGraph SerializedGraph;
	SerializedGraph.Properties.bGenerateIslands = true;
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex{FGuid{0, 0, 0, 1}}, nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex{FGuid{0, 0, 0, 2}}, nullptr });
	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[0], FGraphVertexHandle { FGraphUniqueIndex{FGuid{0, 0, 0, 3}}, nullptr } });

	Graph->LoadFromSerializedGraph(SerializedGraph);
	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(Graph->NumVertices() == 2);
	CHECK(Graph->NumEdges() == 0);
	CHECK(Graph->NumIslands() == 0);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Read::Errors::Island::Invalid Index", "[graph]")
{
	FSerializableGraph SerializedGraph;
	SerializedGraph.Properties.bGenerateIslands = true;
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[0], SerializedGraph.Vertices[1] });
	SerializedGraph.Islands.Add(FGraphIslandHandle { FGraphUniqueIndex{FGuid{0, 0, 0, 0}}, nullptr }, FSerializedIslandData { TArray{SerializedGraph.Vertices[0], SerializedGraph.Vertices[1] } });

	Graph->LoadFromSerializedGraph(SerializedGraph);
	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(Graph->NumVertices() == 2);
	CHECK(Graph->NumEdges() == 1);
	CHECK(Graph->NumIslands() == 1);
	CHECK(Graph->GetIslands().Contains(FGraphIslandHandle { FGraphUniqueIndex { FGuid { 0, 0, 0, 0 } }, nullptr }) == false);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Read::Errors::Island::Invalid Vertex Index", "[graph]")
{
	FSerializableGraph SerializedGraph;
	SerializedGraph.Properties.bGenerateIslands = true;
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex{FGuid{0, 0, 0, 1}}, nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex{FGuid{0, 0, 0, 2}}, nullptr });
	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[0], SerializedGraph.Vertices[1] });
	SerializedGraph.Islands.Add(FGraphIslandHandle { FGraphUniqueIndex{FGuid{1, 0, 0, 0}}, nullptr }, FSerializedIslandData { TArray{ SerializedGraph.Vertices[0], SerializedGraph.Vertices[1], FGraphVertexHandle { FGraphUniqueIndex{FGuid{0, 0, 0, 3}}, nullptr } } });

	Graph->LoadFromSerializedGraph(SerializedGraph);
	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(Graph->NumVertices() == 2);
	CHECK(Graph->NumEdges() == 1);
	CHECK(Graph->NumIslands() == 1);

	UGraphIsland* LoadedIsland = Graph->GetIslands().FindRef(FGraphIslandHandle {FGraphUniqueIndex{FGuid{1, 0, 0, 0}}, nullptr });
	REQUIRE(LoadedIsland != nullptr);
	CHECK(LoadedIsland->Num() == 2);
	CHECK(LoadedIsland->GetVertices().Contains(SerializedGraph.Vertices[0]) == true);
	CHECK(LoadedIsland->GetVertices().Contains(SerializedGraph.Vertices[1]) == true);
	CHECK(LoadedIsland->GetVertices().Contains(FGraphVertexHandle { FGraphUniqueIndex { FGuid { 0, 0, 0, 3 } }, nullptr }) == false);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Read::Errors::Island::Edge Island Mismatch::Merge", "[graph]")
{
	FSerializableGraph SerializedGraph;
	SerializedGraph.Properties.bGenerateIslands = true;
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });

	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[0], SerializedGraph.Vertices[1] });
	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[1], SerializedGraph.Vertices[2] });

	SerializedGraph.Islands.Add(FGraphIslandHandle { FGraphUniqueIndex{FGuid{1, 0, 0, 0}}, nullptr }, FSerializedIslandData { TArray { SerializedGraph.Vertices[0], SerializedGraph.Vertices[1] } });
	SerializedGraph.Islands.Add(FGraphIslandHandle { FGraphUniqueIndex{FGuid{2, 0, 0, 0}}, nullptr }, FSerializedIslandData { TArray { SerializedGraph.Vertices[2] } });

	Graph->LoadFromSerializedGraph(SerializedGraph);
	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(Graph->NumVertices() == 3);
	CHECK(Graph->NumEdges() == 2);
	CHECK(Graph->NumIslands() == 1);

	CHECK(Graph->GetIslands().Contains(FGraphIslandHandle { FGraphUniqueIndex { FGuid { 1, 0, 0, 0 } }, nullptr }) == true);
	CHECK(Graph->GetIslands().Contains(FGraphIslandHandle { FGraphUniqueIndex { FGuid { 2, 0, 0, 0 } }, nullptr }) == false);

	UGraphIsland* LoadedIsland = Graph->GetIslands().FindRef(FGraphIslandHandle {FGraphUniqueIndex{FGuid{1, 0, 0, 0}}, nullptr });
	REQUIRE(LoadedIsland != nullptr);
	CHECK(LoadedIsland->Num() == 3);
	CHECK(LoadedIsland->GetVertices().Contains(SerializedGraph.Vertices[0]) == true);
	CHECK(LoadedIsland->GetVertices().Contains(SerializedGraph.Vertices[1]) == true);
	CHECK(LoadedIsland->GetVertices().Contains(SerializedGraph.Vertices[2]) == true);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Serialization::Read::Errors::Island::Edge Island Mismatch::Split", "[graph]")
{
	FSerializableGraph SerializedGraph;
	SerializedGraph.Properties.bGenerateIslands = true;
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });
	SerializedGraph.Vertices.Add(FGraphVertexHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr });

	SerializedGraph.Edges.Add(FGraphEdgeHandle { FGraphUniqueIndex::CreateUniqueIndex(), nullptr }, FSerializedEdgeData{ SerializedGraph.Vertices[0], SerializedGraph.Vertices[1] });

	SerializedGraph.Islands.Add(FGraphIslandHandle { FGraphUniqueIndex{FGuid{1, 0, 0, 0}}, nullptr }, FSerializedIslandData { TArray { SerializedGraph.Vertices[0], SerializedGraph.Vertices[1], SerializedGraph.Vertices[2] } });

	Graph->LoadFromSerializedGraph(SerializedGraph);
	CHECK(SerializedGraph.Properties == Graph->GetProperties());
	CHECK(Graph->NumVertices() == 3);
	CHECK(Graph->NumEdges() == 1);
	CHECK(Graph->NumIslands() == 2);

	CHECK(Graph->GetIslands().Contains(FGraphIslandHandle { FGraphUniqueIndex { FGuid { 1, 0, 0, 0 } }, nullptr }) == true);

	UGraphIsland* LoadedIsland = Graph->GetIslands().FindRef(FGraphIslandHandle {FGraphUniqueIndex{FGuid{1, 0, 0, 0}}, nullptr });
	REQUIRE(LoadedIsland != nullptr);
	CHECK(LoadedIsland->Num() == 2);
	CHECK(LoadedIsland->GetVertices().Contains(SerializedGraph.Vertices[0]) == true);
	CHECK(LoadedIsland->GetVertices().Contains(SerializedGraph.Vertices[1]) == true);
	CHECK(LoadedIsland->GetVertices().Contains(SerializedGraph.Vertices[2]) == false);

	UGraphIsland* SplitIsland = Graph->GetCompleteNodeHandle(SerializedGraph.Vertices[2]).GetVertex()->GetParentIsland().GetIsland();
	REQUIRE(SplitIsland != nullptr);
	CHECK(SplitIsland->Num() == 1);
	CHECK(SplitIsland->GetVertices().Contains(SerializedGraph.Vertices[0]) == false);
	CHECK(SplitIsland->GetVertices().Contains(SerializedGraph.Vertices[1]) == false);
	CHECK(SplitIsland->GetVertices().Contains(SerializedGraph.Vertices[2]) == true);
}