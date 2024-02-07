// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/Graph.h"

class FTestGraphBuilder
{
public:
	FTestGraphBuilder();
	~FTestGraphBuilder();

	void PopulateVertices(uint32 Total, bool bFinalize);
	void FinalizeVertices();

	void BuildFullyConnectedEdges(int32 NodesPerIsland);
	void BuildLinearEdges(int32 NodesPerIsland);

	void FinalizeEdges();

	TArray<FGraphEdgeHandle> GetEdgesForVertex(const FGraphVertexHandle& Handle) const;

protected:
	TObjectPtr<UGraph> Graph;
	TArray<FGraphVertexHandle> VertexHandles;
	TArray<FGraphIslandHandle> IslandHandles;

	void GraphSanityCheck();
	void IslandVertexParentIslandSanityCheck(const FGraphIslandHandle& IslandHandle);
};