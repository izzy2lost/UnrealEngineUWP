// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/GraphElement.h"
#include "GraphEdge.generated.h"

class UGraphVertex;

UCLASS()
class GAMEPLAYGRAPH_API UGraphEdge : public UGraphElement
{
	GENERATED_BODY()
public:
	UGraphEdge();

	const FGraphVertexHandle& GetOtherNode(const FGraphVertexHandle& InNode) const;
	bool ContainsNode(const FGraphVertexHandle& InNode) const;

	const FGraphVertexHandle& NodeA() const { return A; }
	const FGraphVertexHandle& NodeB() const { return B; }

	FGraphVertexHandle& NodeA() { return A; }
	FGraphVertexHandle& NodeB() { return B; }

	FGraphEdgeHandle Handle() const
	{
		return FGraphEdgeHandle{ GetUniqueIndex(), GetGraph() };
	}

	friend class UGraph;

protected:
	void SetNodes(const FGraphVertexHandle& InA, const FGraphVertexHandle& InB);

private:
	UPROPERTY(SaveGame)
	FGraphVertexHandle A;

	UPROPERTY(SaveGame)
	FGraphVertexHandle B;
};