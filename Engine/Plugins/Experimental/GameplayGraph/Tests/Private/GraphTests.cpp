// Copyright Epic Games, Inc. All Rights Reserved.
#include "TestHarness.h"
#include "TestGraphBuilder.h"

TEST_CASE("Graph::Properties::Equality", "[graph][properties]")
{
	SECTION("Equality")
	{
		FGraphProperties Props1;
		Props1.bGenerateIslands = true;

		FGraphProperties Props2;
		Props2.bGenerateIslands = true;

		CHECK(Props1 == Props2);
	}

	SECTION("InEquality")
	{
		FGraphProperties Props1;
		Props1.bGenerateIslands = true;

		FGraphProperties Props2;
		Props2.bGenerateIslands = false;

		CHECK(Props1 != Props2);
	}
}

TEST_CASE("Graph::Default Constructor", "[graph]")
{
	TObjectPtr<UGraph> Graph = NewObject<UGraph>();

	SECTION("State")
	{
		CHECK(Graph->NumVertices() == 0);
		CHECK(Graph->NumEdges() == 0);
		CHECK(Graph->NumIslands() == 0);
	}
}

TEST_CASE("Graph::Initialize Properties", "[graph]")
{
	TObjectPtr<UGraph> Graph = NewObject<UGraph>();

	SECTION("True Islands")
	{
		FGraphProperties Props1;
		Props1.bGenerateIslands = true;
		Graph->InitializeFromProperties(Props1);
		CHECK(Graph->GetProperties() == Props1);
	}

	SECTION("False Islands")
	{
		FGraphProperties Props1;
		Props1.bGenerateIslands = false;
		Graph->InitializeFromProperties(Props1);
		CHECK(Graph->GetProperties() == Props1);
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Create Vertices", "[graph]")
{
	FGraphUniqueIndex Node1Index = { FGuid { 1, 1, 1, 1 } };
	FGraphVertexHandle Node1 = Graph->CreateVertex(Node1Index);
	{
		CHECK(Node1.GetUniqueIndex() == Node1Index);
		CHECK(Graph->NumVertices() == 1);
		CHECK(Graph->NumEdges() == 0);
		CHECK(Graph->NumIslands() == 0);
		CHECK(Graph->GetVertices().Contains(Node1) == true);
	}

	FGraphUniqueIndex Node2Index = { FGuid { 2, 2, 2, 2 } };
	FGraphVertexHandle Node2 = Graph->CreateVertex(Node2Index);
	{
		CHECK(Node2.GetUniqueIndex() == Node2Index);
		CHECK(Graph->NumVertices() == 2);
		CHECK(Graph->NumEdges() == 0);
		CHECK(Graph->NumIslands() == 0);
		CHECK(Graph->GetVertices().Contains(Node2) == true);
	}

	{
		{
			FGraphVertexHandle Complete1 = Graph->GetCompleteNodeHandle(FGraphVertexHandle{ Node1Index, Graph });
			CHECK(Complete1 == Node1);
			CHECK(Complete1.IsValid() == true);
			CHECK(Complete1.HasElement() == true);
			CHECK(Complete1.IsComplete() == true);
		}

		{
			FGraphVertexHandle Complete2 = Graph->GetCompleteNodeHandle(FGraphVertexHandle{ Node2Index, Graph });
			CHECK(Complete2 == Node2);
			CHECK(Complete2.IsValid() == true);
			CHECK(Complete2.HasElement() == true);
			CHECK(Complete2.IsComplete() == true);
		}

		{
			FGraphVertexHandle Complete1 = Graph->GetCompleteNodeHandle(Node1);
			CHECK(Complete1 == Node1);
			CHECK(Complete1.IsValid() == true);
			CHECK(Complete1.HasElement() == true);
			CHECK(Complete1.IsComplete() == true);
		}

		{
			FGraphVertexHandle Incomplete = Graph->GetCompleteNodeHandle(FGraphVertexHandle{ FGraphUniqueIndex{ FGuid{ 3, 3, 3, 3 } }, Graph });
			CHECK(Incomplete.IsValid() == false);
			CHECK(Incomplete.IsComplete() == false);
			CHECK(Incomplete.HasElement() == false);
		}

		{
			FGraphVertexHandle Incomplete = Graph->GetCompleteNodeHandle(FGraphVertexHandle{ FGraphUniqueIndex{}, Graph });
			CHECK(Incomplete.IsValid() == false);
			CHECK(Incomplete.IsComplete() == false);
			CHECK(Incomplete.HasElement() == false);
		}
	}

	{
		Graph->FinalizeVertex(Node1);
		CHECK(Graph->NumVertices() == 2);
		CHECK(Graph->NumEdges() == 0);
		CHECK(Graph->NumIslands() == 1);
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Create Vertices::Duplicate", "[graph]")
{
	FGraphUniqueIndex Node1Index = { FGuid { 1, 1, 1, 1 } };
	FGraphVertexHandle Node1 = Graph->CreateVertex(Node1Index);
	{
		CHECK(Node1.GetUniqueIndex() == Node1Index);
		CHECK(Graph->NumVertices() == 1);
		CHECK(Graph->NumEdges() == 0);
		CHECK(Graph->NumIslands() == 0);
		CHECK(Graph->GetVertices().Contains(Node1) == true);
	}

	FGraphVertexHandle Node2 = Graph->CreateVertex(Node1Index);
	{
		CHECK(Node2.IsValid() == false);
		CHECK(Node2.IsComplete() == false);
		CHECK(Graph->NumVertices() == 1);
		CHECK(Graph->NumEdges() == 0);
		CHECK(Graph->NumIslands() == 0);
		CHECK(Graph->GetVertices().Contains(Node2) == false);
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Create Edges::Single", "[graph]")
{
	PopulateVertices(4, false);

	{
		FEdgeCreationParameters Params;
		Params.VertexHandle1 = VertexHandles[1];
		Params.VertexHandle2 = VertexHandles[0];
		Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 1, 1, 1, 1 } };

		TArray<FGraphEdgeHandle> Edges;
		Graph->CreateBulkEdges({ Params }, &Edges);
		CHECK(Graph->NumEdges() == 1);
		CHECK(Graph->NumIslands() == 1);

		REQUIRE(Edges.Num() == 1);
		CHECK(Edges[0].GetUniqueIndex() == Params.EdgeIndex);
		CHECK(Edges[0].IsComplete());
		CHECK(Graph->GetEdges().Contains(Edges[0]) == true);
	}

	{
		FinalizeVertices();
		CHECK(Graph->NumEdges() == 1);
		CHECK(Graph->NumIslands() == 3);
	}

	{
		FEdgeCreationParameters Params;
		Params.VertexHandle1 = VertexHandles[2];
		Params.VertexHandle2 = VertexHandles[3];
		Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 2, 2, 2, 2 } };

		TArray<FGraphEdgeHandle> Edges;
		Graph->CreateBulkEdges({ Params }, &Edges);
		CHECK(Graph->NumEdges() == 2);
		CHECK(Graph->NumIslands() == 2);

		REQUIRE(Edges.Num() == 1);
		CHECK(Edges[0].GetUniqueIndex() == Params.EdgeIndex);
		CHECK(Edges[0].IsComplete());
		CHECK(Graph->GetEdges().Contains(Edges[0]) == true);
	}

	{
		FEdgeCreationParameters Params;
		Params.VertexHandle1 = VertexHandles[0];
		Params.VertexHandle2 = VertexHandles[2];
		Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 3, 3, 3, 3 } };

		TArray<FGraphEdgeHandle> Edges;
		Graph->CreateBulkEdges({ Params }, &Edges);
		CHECK(Graph->NumEdges() == 3);
		CHECK(Graph->NumIslands() == 1);

		REQUIRE(Edges.Num() == 1);
		CHECK(Edges[0].GetUniqueIndex() == Params.EdgeIndex);
		CHECK(Graph->GetEdges().Contains(Edges[0]) == true);
	}

	{
		FEdgeCreationParameters Params;
		Params.VertexHandle1 = VertexHandles[0];
		Params.VertexHandle2 = VertexHandles[2];
		Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 4, 4, 4, 4 } };

		TArray<FGraphEdgeHandle> Edges;
		Graph->CreateBulkEdges({ Params }, &Edges);
		CHECK(Graph->NumEdges() == 3);
		CHECK(Graph->NumIslands() == 1);

		REQUIRE(Edges.Num() == 0);
	}

	{
		FEdgeCreationParameters Params;
		Params.VertexHandle1 = VertexHandles[2];
		Params.VertexHandle2 = VertexHandles[0];
		Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 5, 5, 5, 5 } };

		TArray<FGraphEdgeHandle> Edges;
		Graph->CreateBulkEdges({ Params }, &Edges);
		CHECK(Graph->NumEdges() == 3);
		CHECK(Graph->NumIslands() == 1);

		REQUIRE(Edges.Num() == 0);
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Create Edges::Bulk", "[graph]")
{
	PopulateVertices(4, true);

	{
		FEdgeCreationParameters Params1;
		Params1.VertexHandle1 = VertexHandles[1];
		Params1.VertexHandle2 = VertexHandles[0];
		Params1.EdgeIndex = FGraphUniqueIndex{ FGuid{ 1, 1, 1, 1 } };

		FEdgeCreationParameters Params2;
		Params2.VertexHandle1 = VertexHandles[2];
		Params2.VertexHandle2 = VertexHandles[3];
		Params2.EdgeIndex = FGraphUniqueIndex{ FGuid{ 2, 2, 2, 2 } };

		FEdgeCreationParameters Params3;
		Params3.VertexHandle1 = VertexHandles[0];
		Params3.VertexHandle2 = VertexHandles[2];
		Params3.EdgeIndex = FGraphUniqueIndex{ FGuid{ 3, 3, 3, 3 } };

		TArray<FGraphEdgeHandle> Edges;
		Graph->CreateBulkEdges({ Params1, Params2, Params3 }, &Edges);
		CHECK(Graph->NumEdges() == 3);
		CHECK(Graph->NumIslands() == 1);

		REQUIRE(Edges.Num() == 3);
		CHECK(Edges[0].GetUniqueIndex() == Params1.EdgeIndex);
		CHECK(Edges[1].GetUniqueIndex() == Params2.EdgeIndex);
		CHECK(Edges[2].GetUniqueIndex() == Params3.EdgeIndex);
		CHECK(Graph->GetEdges().Contains(Edges[0]) == true);
		CHECK(Graph->GetEdges().Contains(Edges[1]) == true);
		CHECK(Graph->GetEdges().Contains(Edges[2]) == true);
		CHECK(Edges[0].IsComplete() == true);
		CHECK(Edges[1].IsComplete() == true);
		CHECK(Edges[2].IsComplete() == true);
	}

	{
		FEdgeCreationParameters Params1;
		Params1.VertexHandle1 = VertexHandles[0];
		Params1.VertexHandle2 = VertexHandles[1];
		Params1.EdgeIndex = FGraphUniqueIndex{ FGuid{ 4, 4, 4, 4 } };

		FEdgeCreationParameters Params2;
		Params2.VertexHandle1 = VertexHandles[3];
		Params2.VertexHandle2 = VertexHandles[2];
		Params2.EdgeIndex = FGraphUniqueIndex{ FGuid{ 5, 5, 5, 5 } };

		FEdgeCreationParameters Params3;
		Params3.VertexHandle1 = VertexHandles[2];
		Params3.VertexHandle2 = VertexHandles[0];
		Params3.EdgeIndex = FGraphUniqueIndex{ FGuid{ 6, 6, 6, 6 } };

		TArray<FGraphEdgeHandle> Edges;
		Graph->CreateBulkEdges({ Params1, Params2, Params3 }, &Edges);
		CHECK(Graph->NumEdges() == 3);
		CHECK(Graph->NumIslands() == 1);
		CHECK(Edges.Num() == 0);
	}
}

class FScopedVertexEdgeCleanupChecker
{
public:
	template<typename TIterable>
	FScopedVertexEdgeCleanupChecker(UGraph* InGraph, const TIterable& InVertexHandles)
	: Graph(InGraph)
	{
		REQUIRE(Graph != nullptr);

		for (const FGraphVertexHandle& VertexHandle : InVertexHandles)
		{
			UGraphVertex* Vertex = VertexHandle.GetVertex();
			REQUIRE(Vertex != nullptr);

			Vertex->ForEachAdjacentVertex(
				[this](const FGraphVertexHandle& NeighborHandle, const FGraphEdgeHandle& EdgeHandle)
				{
					EdgeHandles.Add(EdgeHandle);
					CHECK(Graph->GetEdges().Contains(EdgeHandle) == true);
					CHECK(EdgeHandle.IsComplete() == true);
				}
			);
		}
	}

	~FScopedVertexEdgeCleanupChecker()
	{
		for (const FGraphEdgeHandle& EdgeHandle : EdgeHandles)
		{
			CHECK(Graph->GetEdges().Contains(EdgeHandle) == false);
			CHECK(EdgeHandle.IsComplete() == false);
		}
	}

private:
	UGraph* Graph;
	TArray<FGraphEdgeHandle> EdgeHandles;
};

class FScopedVertexIslandCleanupChecker
{
public:
	template<typename TIterable>
	FScopedVertexIslandCleanupChecker(UGraph* InGraph, const TIterable& InVertexHandles)
	: Graph(InGraph)
	{
		REQUIRE(Graph != nullptr);

		for (const FGraphVertexHandle& VertexHandle : InVertexHandles)
		{
			UGraphVertex* Vertex = VertexHandle.GetVertex();
			REQUIRE(Vertex != nullptr);

			FGraphIslandHandle ParentIslandHandle = Vertex->GetParentIsland();
			CHECK(Graph->GetIslands().Contains(ParentIslandHandle) == true);
			CHECK(ParentIslandHandle.IsComplete() == true);
			IslandHandles.Add(ParentIslandHandle);
		}
	}

	~FScopedVertexIslandCleanupChecker()
	{
		for (const FGraphIslandHandle& IslandHandle : IslandHandles)
		{
			CHECK(Graph->GetIslands().Contains(IslandHandle) == false);
			CHECK(IslandHandle.IsComplete() == false);
		}
	}

private:
	UGraph* Graph;
	TArray<FGraphIslandHandle> IslandHandles;
};

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Remove Vertex::Single::Fully Connected", "[graph]")
{
	PopulateVertices(5, true);
	BuildFullyConnectedEdges(5);

	CHECK(Graph->NumVertices() == 5);
	CHECK(Graph->NumEdges() == 10);
	CHECK(Graph->NumIslands() == 1);

	{
		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[0] } };
		Graph->RemoveVertex(VertexHandles[0]);
	}

	CHECK(VertexHandles[0].IsComplete() == false);
	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[0]).IsComplete() == false);
	CHECK(Graph->GetVertices().Contains(VertexHandles[0]) == false);
	CHECK(Graph->NumVertices() == 4);
	CHECK(Graph->NumEdges() == 6);
	CHECK(Graph->NumIslands() == 1);

	{
		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[1] } };
		Graph->RemoveVertex(VertexHandles[1]);
	}

	CHECK(VertexHandles[1].IsComplete() == false);
	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[1]).IsComplete() == false);
	CHECK(Graph->GetVertices().Contains(VertexHandles[1]) == false);
	CHECK(Graph->NumVertices() == 3);
	CHECK(Graph->NumEdges() == 3);
	CHECK(Graph->NumIslands() == 1);

	{
		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[2] } };
		Graph->RemoveVertex(VertexHandles[2]);
	}

	CHECK(VertexHandles[2].IsComplete() == false);
	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[2]).IsComplete() == false);
	CHECK(Graph->GetVertices().Contains(VertexHandles[2]) == false);
	CHECK(Graph->NumVertices() == 2);
	CHECK(Graph->NumEdges() == 1);
	CHECK(Graph->NumIslands() == 1);

	{
		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[3] } };
		Graph->RemoveVertex(VertexHandles[3]);
	}

	CHECK(VertexHandles[3].IsComplete() == false);
	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[3]).IsComplete() == false);
	CHECK(Graph->GetVertices().Contains(VertexHandles[3]) == false);
	CHECK(Graph->NumVertices() == 1);
	CHECK(Graph->NumEdges() == 0);
	CHECK(Graph->NumIslands() == 1);

	{
		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[4] } };
		Graph->RemoveVertex(VertexHandles[4]);
	}

	CHECK(VertexHandles[4].IsComplete() == false);
	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[4]).IsComplete() == false);
	CHECK(Graph->GetVertices().Contains(VertexHandles[4]) == false);
	CHECK(Graph->NumVertices() == 0);
	CHECK(Graph->NumEdges() == 0);
	CHECK(Graph->NumIslands() == 0);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Remove Vertex::Single::Linear", "[graph]")
{
	PopulateVertices(10, true);
	BuildLinearEdges(10);

	CHECK(Graph->NumVertices() == 10);
	CHECK(Graph->NumEdges() == 9);
	CHECK(Graph->NumIslands() == 1);

	SECTION("First Vertex")
	{
		CHECK(Graph->GetVertices().Contains(VertexHandles[0]) == true);
		CHECK(VertexHandles[0].IsComplete() == true);

		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[0] } };
		Graph->RemoveVertex(VertexHandles[0]);

		CHECK(Graph->GetVertices().Contains(VertexHandles[0]) == false);
		CHECK(VertexHandles[0].IsComplete() == false);

		CHECK(Graph->NumVertices() == 9);
		CHECK(Graph->NumEdges() == 8);
		CHECK(Graph->NumIslands() == 1);
	}

	SECTION("Split Island")
	{
		CHECK(Graph->GetVertices().Contains(VertexHandles[5]) == true);
		CHECK(VertexHandles[5].IsComplete() == true);

		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[5] } };
		Graph->RemoveVertex(VertexHandles[5]);

		CHECK(Graph->GetVertices().Contains(VertexHandles[5]) == false);
		CHECK(VertexHandles[5].IsComplete() == false);

		CHECK(Graph->NumVertices() == 9);
		CHECK(Graph->NumEdges() == 7);
		CHECK(Graph->NumIslands() == 2);
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Remove Vertex::Bulk::Fully Connected", "[graph]")
{
	PopulateVertices(5, true);
	BuildFullyConnectedEdges(5);

	CHECK(Graph->NumVertices() == 5);
	CHECK(Graph->NumEdges() == 10);
	CHECK(Graph->NumIslands() == 1);

	{
		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[0], VertexHandles[1] } };
		Graph->RemoveBulkVertices( { VertexHandles[0], VertexHandles[1] });
	}

	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[0]).IsComplete() == false);
	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[1]).IsComplete() == false);
	CHECK(VertexHandles[0].IsComplete() == false);
	CHECK(VertexHandles[1].IsComplete() == false);
	CHECK(Graph->NumVertices() == 3);
	CHECK(Graph->NumEdges() == 3);
	CHECK(Graph->NumIslands() == 1);

	{
		FScopedVertexEdgeCleanupChecker Checker { Graph, TArray{ VertexHandles[2], VertexHandles[3], VertexHandles[4] } };
		Graph->RemoveBulkVertices( { VertexHandles[2], VertexHandles[3], VertexHandles[4] });
	}

	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[2]).IsComplete() == false);
	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[3]).IsComplete() == false);
	CHECK(Graph->GetCompleteNodeHandle(VertexHandles[4]).IsComplete() == false);
	CHECK(VertexHandles[2].IsComplete() == false);
	CHECK(VertexHandles[3].IsComplete() == false);
	CHECK(VertexHandles[4].IsComplete() == false);
	CHECK(Graph->NumVertices() == 0);
	CHECK(Graph->NumEdges() == 0);
	CHECK(Graph->NumIslands() == 0);
}


TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Remove Vertex::Bulk::Linear", "[graph]")
{
	PopulateVertices(10, true);
	BuildLinearEdges(10);

	CHECK(Graph->NumVertices() == 10);
	CHECK(Graph->NumEdges() == 9);
	CHECK(Graph->NumIslands() == 1);

	TArray<FGraphVertexHandle> VerticesToRemove = { VertexHandles[1], VertexHandles[2], VertexHandles[8] };
	for (const FGraphVertexHandle& VertexHandle : VerticesToRemove)
	{
		CHECK(Graph->GetVertices().Contains(VertexHandle) == true);
		CHECK(VertexHandle.IsComplete() == true);
	}
	FScopedVertexEdgeCleanupChecker Checker { Graph, VerticesToRemove };

	Graph->RemoveBulkVertices(VerticesToRemove);
	for (const FGraphVertexHandle& VertexHandle : VerticesToRemove)
	{
		CHECK(Graph->GetVertices().Contains(VertexHandle) == false);
		CHECK(VertexHandle.IsComplete() == false);
	}

	CHECK(Graph->NumVertices() == 7);
	CHECK(Graph->NumEdges() == 4);
	CHECK(Graph->NumIslands() == 3);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Remove Vertex::Incomplete Handle", "[graph]")
{
	PopulateVertices(5, true);
	BuildLinearEdges(5);

	FGraphVertexHandle RemoveHandle{ VertexHandles[0].GetUniqueIndex(), nullptr };
	CHECK(Graph->NumVertices() == 5);
	CHECK(Graph->NumEdges() == 4);
	CHECK(Graph->NumIslands() == 1);

	UGraphIsland* Island = IslandHandles[0].GetIsland();
	REQUIRE(Island != nullptr);
	CHECK(Island->Num() == 5);

	Graph->RemoveVertex(RemoveHandle);

	CHECK(Graph->NumVertices() == 4);
	CHECK(Graph->NumEdges() == 3);
	CHECK(Graph->NumIslands() == 1);
	CHECK(Island->Num() == 4);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Remove Island", "[graph]")
{
	PopulateVertices(10, true);
	BuildFullyConnectedEdges(5);

	CHECK(Graph->NumVertices() == 10);
	CHECK(Graph->NumEdges() == 20);
	CHECK(Graph->NumIslands() == 2);

	{
		CHECK(Graph->GetIslands().Contains(IslandHandles[0]) == true);
		CHECK(IslandHandles[0].IsComplete() == true);
		TSet<FGraphVertexHandle> IslandVertices = IslandHandles[0].GetIsland()->GetVertices();
		FScopedVertexEdgeCleanupChecker EdgeChecker { Graph, IslandVertices };
		FScopedVertexIslandCleanupChecker IslandChecker { Graph, IslandVertices };
		for (const FGraphVertexHandle& VertexHandle : IslandVertices)
		{
			CHECK(Graph->GetCompleteNodeHandle(VertexHandle).IsComplete() == true);
			CHECK(Graph->GetVertices().Contains(VertexHandle) == true);
		}
		Graph->RemoveIsland(IslandHandles[0]);
		for (const FGraphVertexHandle& VertexHandle : IslandVertices)
		{
			CHECK(Graph->GetCompleteNodeHandle(VertexHandle).IsComplete() == false);
			CHECK(Graph->GetVertices().Contains(VertexHandle) == false);
		}

		CHECK(Graph->GetIslands().Contains(IslandHandles[0]) == false);
		CHECK(IslandHandles[0].IsComplete() == false);
	}

	CHECK(Graph->NumVertices() == 5);
	CHECK(Graph->NumEdges() == 10);
	CHECK(Graph->NumIslands() == 1);

	{
		CHECK(Graph->GetIslands().Contains(IslandHandles[1]) == true);
		CHECK(IslandHandles[1].IsComplete() == true);
		TSet<FGraphVertexHandle> IslandVertices = IslandHandles[1].GetIsland()->GetVertices();
		FScopedVertexEdgeCleanupChecker Checker { Graph, IslandVertices };
		FScopedVertexIslandCleanupChecker IslandChecker { Graph, IslandVertices };
		for (const FGraphVertexHandle& VertexHandle : IslandVertices)
		{
			CHECK(Graph->GetCompleteNodeHandle(VertexHandle).IsComplete() == true);
			CHECK(Graph->GetVertices().Contains(VertexHandle) == true);
		}
		Graph->RemoveIsland(IslandHandles[1]);
		for (const FGraphVertexHandle& VertexHandle : IslandVertices)
		{
			CHECK(Graph->GetCompleteNodeHandle(VertexHandle).IsComplete() == false);
			CHECK(Graph->GetVertices().Contains(VertexHandle) == false);
		}

		CHECK(Graph->GetIslands().Contains(IslandHandles[1]) == false);
		CHECK(IslandHandles[1].IsComplete() == false);
	}

	CHECK(Graph->NumVertices() == 0);
	CHECK(Graph->NumEdges() == 0);
	CHECK(Graph->NumIslands() == 0);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Remove Edge::Handle Islands", "[graph]")
{
	PopulateVertices(10, true);
	BuildLinearEdges(10);

	CHECK(Graph->NumVertices() == 10);
	CHECK(Graph->NumEdges() == 9);
	CHECK(Graph->NumIslands() == 1);

	SECTION("First Vertex")
	{
		TArray<FGraphEdgeHandle> Edges = GetEdgesForVertex(VertexHandles[0]);
		for (const FGraphEdgeHandle& EdgeHandle : Edges)
		{
			CHECK(Graph->GetEdges().Contains(EdgeHandle) == true);
			CHECK(EdgeHandle.IsComplete() == true);

			Graph->RemoveEdge(EdgeHandle);

			CHECK(Graph->GetEdges().Contains(EdgeHandle) == false);
			CHECK(EdgeHandle.IsComplete() == false);
		}

		CHECK(Graph->NumVertices() == 10);
		CHECK(Graph->NumEdges() == 8);
		CHECK(Graph->NumIslands() == 2);
	}

	SECTION("Split Island")
	{
		TArray<FGraphEdgeHandle> Edges = GetEdgesForVertex(VertexHandles[5]);
		for (const FGraphEdgeHandle& EdgeHandle : Edges)
		{
			CHECK(Graph->GetEdges().Contains(EdgeHandle) == true);
			CHECK(EdgeHandle.IsComplete() == true);

			Graph->RemoveEdge(EdgeHandle);

			CHECK(Graph->GetEdges().Contains(EdgeHandle) == false);
			CHECK(EdgeHandle.IsComplete() == false);
		}

		CHECK(Graph->NumVertices() == 10);
		CHECK(Graph->NumEdges() == 7);
		CHECK(Graph->NumIslands() == 3);
	}
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Event::Vertex Created", "[graph]")
{
	FGraphUniqueIndex Node1Index = { FGuid { 1, 1, 1, 1 } };

	FGraphVertexHandle CreatedHandle;
	Graph->OnVertexCreated.AddLambda(
		[&CreatedHandle, &Node1Index](const FGraphVertexHandle& InCreatedHandle)
		{
			CHECK(InCreatedHandle.IsComplete());
			CHECK(InCreatedHandle.GetUniqueIndex() == Node1Index);
			CreatedHandle = InCreatedHandle;
		}
	);

	FGraphVertexHandle Node1 = Graph->CreateVertex(Node1Index);
	CHECK(CreatedHandle == Node1);
	CHECK(CreatedHandle.IsComplete() == true);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Event::Edge Created::Single", "[graph]")
{
	PopulateVertices(10, true);

	FEdgeCreationParameters Params;
	Params.VertexHandle1 = VertexHandles[1];
	Params.VertexHandle2 = VertexHandles[0];
	Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 1, 1, 1, 1 } };

	FGraphEdgeHandle CreatedHandle;
	Graph->OnEdgeCreated.AddLambda(
		[&CreatedHandle, &Params](const FGraphEdgeHandle& InCreatedHandle)
		{
			CHECK(InCreatedHandle.IsComplete());
			CHECK(InCreatedHandle.GetUniqueIndex() == Params.EdgeIndex);
			CreatedHandle = InCreatedHandle;
		}
	);

	TArray<FGraphEdgeHandle> Edges;
	Graph->CreateBulkEdges({ Params }, &Edges);

	REQUIRE(Edges.Num() == 1);
	CHECK(Edges[0] == CreatedHandle);
}

TEST_CASE_METHOD(FTestGraphBuilder, "Graph::Event::Island Created", "[graph]")
{
	PopulateVertices(10, false);

	FEdgeCreationParameters Params;
	Params.VertexHandle1 = VertexHandles[1];
	Params.VertexHandle2 = VertexHandles[0];
	Params.EdgeIndex = FGraphUniqueIndex{ FGuid{ 1, 1, 1, 1 } };

	FGraphIslandHandle CreatedHandle;
	Graph->OnIslandCreated.AddLambda(
		[&CreatedHandle](const FGraphIslandHandle& InCreatedHandle)
		{
			CHECK(InCreatedHandle.IsComplete());
			CreatedHandle = InCreatedHandle;
		}
	);

	Graph->CreateBulkEdges({ Params });
	CHECK(CreatedHandle.IsComplete());

	UGraphVertex* Vertex1 = VertexHandles[1].GetVertex();
	REQUIRE(Vertex1 != nullptr);
	CHECK(Vertex1->GetParentIsland() == CreatedHandle);

	UGraphVertex* Vertex0 = VertexHandles[0].GetVertex();
	REQUIRE(Vertex0 != nullptr);
	CHECK(Vertex0->GetParentIsland() == CreatedHandle);
}