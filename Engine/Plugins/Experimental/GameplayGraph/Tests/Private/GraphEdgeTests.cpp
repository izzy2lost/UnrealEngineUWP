// Copyright Epic Games, Inc. All Rights Reserved.
#include "TestHarness.h"
#include "TestGraphBuilder.h"

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Edge::Create", "[graph][edge]")
{
	PopulateVertices(3, true);

	FEdgeCreationParameters Params;
	Params.VertexHandle1 = VertexHandles[1];
	Params.VertexHandle2 = VertexHandles[0];
	Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 1, 1, 1, 1 } };

	TArray<FGraphEdgeHandle> Edges;
	Graph->CreateBulkEdges({ Params }, &Edges);
	REQUIRE(Edges.Num() == 1);

	SECTION("Graph Edge Properties")
	{
		CHECK(Edges[0].HasElement());
		CHECK(Edges[0].IsValid());

		UGraphEdge* E = Edges[0].GetEdge();
		REQUIRE(E != nullptr);
		CHECK(E->NodeA() == VertexHandles[0]);
		CHECK(E->NodeB() == VertexHandles[1]);
		CHECK(E->Handle() == Edges[0]);
		CHECK(E->ContainsNode(VertexHandles[0]) == true);
		CHECK(E->ContainsNode(VertexHandles[1]) == true);
		CHECK(E->ContainsNode(VertexHandles[2]) == false);
		CHECK(E->GetOtherNode(VertexHandles[0]) == VertexHandles[1]);
		CHECK(E->GetOtherNode(VertexHandles[1]) == VertexHandles[0]);
		CHECK(E->GetOtherNode(VertexHandles[2]) == FGraphVertexHandle{});

		FSerializedEdgeData Serialized = E->GetSerializedData();
		CHECK(Serialized.Node1 == VertexHandles[0]);
		CHECK(Serialized.Node2 == VertexHandles[1]);
	}

	SECTION("Remove Edge")
	{
		CHECK(Edges[0].IsValid() == true);
		CHECK(Edges[0].IsComplete() == true);
		CHECK(Edges[0].HasElement() == true);
		CHECK(Edges[0].GetUniqueIndex() == Params.EdgeIndex);
		CHECK(Edges[0].GetGraph() == Graph);

		Graph->RemoveEdge(Edges[0]);

		CHECK(Edges[0].IsValid() == true);
		CHECK(Edges[0].IsComplete() == false);
		CHECK(Edges[0].HasElement() == false);
		CHECK(Edges[0].GetUniqueIndex() == Params.EdgeIndex);
		CHECK(Edges[0].GetGraph() == Graph);
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Edge::Remove Vertex", "[graph][edge]")
{
	PopulateVertices(3, true);

	FEdgeCreationParameters Params;
	Params.VertexHandle1 = VertexHandles[1];
	Params.VertexHandle2 = VertexHandles[0];
	Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 1, 1, 1, 1 } };

	TArray<FGraphEdgeHandle> Edges;
	Graph->CreateBulkEdges({ Params }, &Edges);
	REQUIRE(Edges.Num() == 1);

	CHECK(Edges[0].IsComplete() == true);
	Graph->RemoveVertex(VertexHandles[0]);
	CHECK(Edges[0].IsComplete() == false);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Edge::Remove Edge", "[graph][edge]")
{
	PopulateVertices(3, true);

	FEdgeCreationParameters Params;
	Params.VertexHandle1 = VertexHandles[1];
	Params.VertexHandle2 = VertexHandles[0];
	Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 1, 1, 1, 1 } };

	TArray<FGraphEdgeHandle> Edges;
	Graph->CreateBulkEdges({ Params }, &Edges);
	REQUIRE(Edges.Num() == 1);

	SECTION("With islands")
	{
		CHECK(Edges[0].IsComplete() == true);
		Graph->RemoveEdge(Edges[0]);
		CHECK(Edges[0].IsComplete() == false);
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Edge::Remove island", "[graph][edge]")
{
	PopulateVertices(3, true);

	FEdgeCreationParameters Params;
	Params.VertexHandle1 = VertexHandles[1];
	Params.VertexHandle2 = VertexHandles[0];
	Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 1, 1, 1, 1 } };

	TArray<FGraphEdgeHandle> Edges;
	CHECK(Graph->GetIslands().Num() == 3);
	Graph->CreateBulkEdges({ Params }, &Edges);
	REQUIRE(Edges.Num() == 1);
	REQUIRE(Graph->GetIslands().Num() == 2);

	FGraphIslandHandle IslandHandle = Graph->GetIslands().CreateConstIterator()->Key;

	CHECK(Edges[0].IsComplete() == true);
	Graph->RemoveIsland(IslandHandle);
	CHECK(Edges[0].IsComplete() == false);
}