// Copyright Epic Games, Inc. All Rights Reserved.
#include "TestGraphBuilder.h"

#include "TestHarness.h"

FTestGraphBuilder::FTestGraphBuilder()
{
	Graph = NewObject<UGraph>();
	Graph->InitializeFromProperties(FGraphProperties{ true });
}

FTestGraphBuilder::~FTestGraphBuilder()
{
	GraphSanityCheck();
}

void FTestGraphBuilder::PopulateVertices(uint32 Total, bool bFinalize)
{
	for (uint32 Count = 1; Count <= Total; ++Count)
	{
		FGraphUniqueIndex NodeIndex = { FGuid { 0, 0, 0, Count } };
		FGraphVertexHandle Node = Graph->CreateVertex(NodeIndex);
		VertexHandles.Add(Node);
	}

	if (bFinalize)
	{
		FinalizeVertices();
	}
}

void FTestGraphBuilder::FinalizeVertices()
{
	for (uint32 Count = 0; Count < static_cast<uint32>(VertexHandles.Num()); ++Count)
	{
		Graph->FinalizeVertex(VertexHandles[Count]);
	}
}

void FTestGraphBuilder::BuildFullyConnectedEdges(int32 NodesPerIsland)
{
	for (int32 NodeIndex = 0; NodeIndex < VertexHandles.Num(); NodeIndex += NodesPerIsland)
	{
		TArray<FEdgeCreationParameters> AllEdges;
		for (int32 SourceOffset = 0; SourceOffset < NodesPerIsland; ++SourceOffset)
		{
			if ((NodeIndex + SourceOffset) >= VertexHandles.Num())
			{
				break;
			}

			for (int32 DestOffset = SourceOffset + 1; DestOffset < NodesPerIsland; ++DestOffset)
			{
				if ((NodeIndex + DestOffset) >= VertexHandles.Num())
				{
					break;
				}

				FEdgeCreationParameters Params;
				Params.VertexHandle1 = VertexHandles[NodeIndex + SourceOffset];
				Params.VertexHandle2 = VertexHandles[NodeIndex + DestOffset];
				AllEdges.Add(Params);
			}
		}

		Graph->CreateBulkEdges(MoveTemp(AllEdges));
	}

	FinalizeEdges();
}

void FTestGraphBuilder::BuildLinearEdges(int32 NodesPerIsland)
{
	for (int32 NodeIndex = 0; NodeIndex < VertexHandles.Num(); NodeIndex += NodesPerIsland)
	{
		TArray<FEdgeCreationParameters> AllEdges;
		for (int32 SourceOffset = 1; SourceOffset < NodesPerIsland; ++SourceOffset)
		{
			if ((NodeIndex + SourceOffset) >= VertexHandles.Num())
			{
				break;
			}

			FEdgeCreationParameters Params;
			Params.VertexHandle1 = VertexHandles[NodeIndex + SourceOffset - 1];
			Params.VertexHandle2 = VertexHandles[NodeIndex + SourceOffset];
			AllEdges.Add(Params);
		}

		Graph->CreateBulkEdges(MoveTemp(AllEdges));
	}

	FinalizeEdges();
}

void FTestGraphBuilder::FinalizeEdges()
{
	IslandHandles.Empty();
	for (const TPair<FGraphIslandHandle, TObjectPtr<UGraphIsland>>& Pair : Graph->GetIslands())
	{
		IslandHandles.Add(Pair.Key);
	}
}

TArray<FGraphEdgeHandle> FTestGraphBuilder::GetEdgesForVertex(const FGraphVertexHandle& Handle) const
{
	TArray<FGraphEdgeHandle> Edges;
	if (UGraphVertex* Vertex = Handle.GetVertex())
	{
		Edges.Reserve(Vertex->NumEdges());
		Vertex->ForEachAdjacentVertex(
			[&Edges](const FGraphVertexHandle& NeighborVertexHandle, const FGraphEdgeHandle& EdgeHandle)
			{
				Edges.Add(EdgeHandle);
			}
		);
	}
	return Edges;
}

void FTestGraphBuilder::GraphSanityCheck()
{
	for (const TPair<FGraphVertexHandle, TObjectPtr<UGraphVertex>>& Data : Graph->GetVertices())
	{
		CHECK(Data.Key.IsComplete() == true);
		CHECK(Data.Value != nullptr);
		CHECK(Data.Value == Data.Key.GetVertex());
		CHECK(Data.Key == Data.Value->Handle());
		CHECK(Data.Key.GetUniqueIndex().IsTemporary() == false);
		CHECK(Data.Key.GetGraph() == Graph);

		if (Data.Value->GetParentIsland().IsValid())
		{
			CHECK(Data.Value->GetParentIsland().IsComplete());
			CHECK(Data.Value->GetParentIsland().GetIsland() == Graph->GetIslands().FindRef(Data.Value->GetParentIsland()));
		}

		Data.Value->ForEachAdjacentVertex(
			[this](const FGraphVertexHandle& NeighborVertexHandle, const FGraphEdgeHandle& EdgeHandle)
			{
				CHECK(NeighborVertexHandle.IsComplete() == true);
				CHECK(NeighborVertexHandle.GetVertex() == Graph->GetVertices().FindRef(NeighborVertexHandle));

				CHECK(EdgeHandle.IsComplete() == true);
				CHECK(EdgeHandle.GetEdge() == Graph->GetEdges().FindRef(EdgeHandle));
			}
		);
	}

	for (const TPair<FGraphEdgeHandle, TObjectPtr<UGraphEdge>>& Data : Graph->GetEdges())
	{
		CHECK(Data.Key.IsComplete() == true);
		CHECK(Data.Value != nullptr);
		CHECK(Data.Value == Data.Key.GetEdge());
		CHECK(Data.Key == Data.Value->Handle());
		CHECK(Data.Key.GetUniqueIndex().IsTemporary() == false);
		CHECK(Data.Key.GetGraph() == Graph);

		CHECK(Data.Value->NodeA().IsComplete() == true);
		CHECK(Data.Value->NodeA().GetVertex() == Graph->GetVertices().FindRef(Data.Value->NodeA()));

		CHECK(Data.Value->NodeB().IsComplete() == true);
		CHECK(Data.Value->NodeB().GetVertex() == Graph->GetVertices().FindRef(Data.Value->NodeB()));
	}

	for (const TPair<FGraphIslandHandle, TObjectPtr<UGraphIsland>>& Data : Graph->GetIslands())
	{
		CHECK(Data.Key.IsComplete() == true);
		CHECK(Data.Value != nullptr);
		CHECK(Data.Value == Data.Key.GetIsland());
		CHECK(Data.Key == Data.Value->Handle());
		CHECK(Data.Key.GetUniqueIndex().IsTemporary() == false);
		CHECK(Data.Key.GetGraph() == Graph);

		for (const FGraphVertexHandle& VertexHandle : Data.Value->GetVertices())
		{
			CHECK(VertexHandle.IsComplete() == true);
			CHECK(VertexHandle.GetVertex() == Graph->GetVertices().FindRef(VertexHandle));
		}

		IslandVertexParentIslandSanityCheck(Data.Key);
	}
}

void FTestGraphBuilder::IslandVertexParentIslandSanityCheck(const FGraphIslandHandle& IslandHandle)
{
	UGraphIsland* Island = IslandHandle.GetIsland();
	REQUIRE(Island != nullptr);

	Island->ForEachVertex(
		[&IslandHandle](const FGraphVertexHandle& VertexHandle)
		{
			CHECK(VertexHandle.GetVertex()->GetParentIsland() == IslandHandle);
		}
	);
}
