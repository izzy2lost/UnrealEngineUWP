// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Graph/GraphEdge.h"
#include "Graph/GraphHandle.h"
#include "Graph/GraphIsland.h"
#include "Graph/GraphVertex.h"
#include "Templates/SubclassOf.h"

#include "Graph.generated.h"

class IGraphSerialization;
class IGraphDeserialization;

USTRUCT()
struct FGraphProperties
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	bool bGenerateIslands = true;
	
	// Comparison operators
	friend bool operator==(const FGraphProperties& Lhs, const FGraphProperties& Rhs) = default;
	friend bool operator!=(const FGraphProperties& Lhs, const FGraphProperties& Rhs) = default;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphVertexCreated, const FGraphVertexHandle&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphEdgeCreated, const FGraphEdgeHandle&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGraphIslandCreated, const FGraphIslandHandle&);

struct FEdgeCreationParameters
{
	FGraphVertexHandle VertexHandle1;
	FGraphVertexHandle VertexHandle2;
	FGraphUniqueIndex EdgeIndex = FGraphUniqueIndex::CreateUniqueIndex();
};

/**
 * A UGraph is a collection of nodes and edges. This graph representation
 * is meant to be easily integrable into gameplay systems in the Unreal Engine.
 * 
 * Conceptually, you can imagine that a graph is meant to easily represent relationships
 * so we can answer queries such as:
 *	- Are these two nodes connected to each other?
 *  - How far away are these two nodes?
 *  - Who is the closest node that has XYZ?
 *  - etc.
 * 
 * UGraph provides an interface to be able to run such queries. However, ultimately what
 * makes the graph useful is not only the relationships represented by the edges, but also
 * the data that is stored on each node and each edge. Depending on what the user wants to
 * represent, the user will have to subclass UGraphVertex and UGraphEdge to hold that data.
 * 
 * As the user adds nodes and edges into the graph, they will also be implicitly creating "islands"
 * (i.e. a connected component in the graph). Each graph may have multiple islands. Users
 * of the graph can disable the island detection/creation if needed.
 * 
 * Note that this is an UNDIRECTED GRAPH.
 */
UCLASS()
class GAMEPLAYGRAPH_API UGraph: public UObject
{
	GENERATED_BODY()
public:
	UGraph() = default;

	void Empty();
	void InitializeFromProperties(const FGraphProperties& Properties);

	/** Given a node handle, find the handle in the current graph with a proper element set. */
	FGraphVertexHandle GetCompleteNodeHandle(const FGraphVertexHandle& InHandle) const;

	/** Create a node with the specified subclass, adds it to the graph, and returns a handle to it. */
	FGraphVertexHandle CreateVertex(FGraphUniqueIndex InUniqueIndex = FGraphUniqueIndex::CreateUniqueIndex(false));

	/** Creates edges in bulk. This is more efficient than calling CreateEdge multiple times since we will only try to assign a node to an island once. */
	void CreateBulkEdges(TArray<FEdgeCreationParameters>&& NodesToConnect, TArray<FGraphEdgeHandle>* OutEdges = nullptr);

	/** Removes a node from the graph along with any edges that contain it. */
	void RemoveVertex(const FGraphVertexHandle& NodeHandle);

	void RemoveBulkVertices(const TArray<FGraphVertexHandle>& InHandles);

	/** Removes an edge from the graph. */
	void RemoveEdge(const FGraphEdgeHandle& EdgeHandle);

	/** This should be called immediately after a node and any relevant edges have been added to the graph. */
	virtual void FinalizeVertex(const FGraphVertexHandle& InHandle);

	/** Remove an island from the graph. */
	void RemoveIsland(const FGraphIslandHandle& IslandHandle);

	/** Refresh the connectivity of the given island (re-check to see whether it should be split). */
	void RefreshIslandConnectivity(const FGraphIslandHandle& IslandHandle);

	int32 NumVertices() const { return Vertices.Num(); }
	int32 NumEdges() const { return Edges.Num(); }
	int32 NumIslands() const { return Islands.Num(); }

	template<typename TLambda>
	void ForEachIsland(TLambda&& Lambda)
	{
		for (const TPair<FGraphIslandHandle, TObjectPtr<UGraphIsland>>& Kvp : Islands)
		{
			Lambda(Kvp.Key, Kvp.Value);
		}
	}

	FOnGraphVertexCreated OnVertexCreated;
	FOnGraphEdgeCreated OnEdgeCreated;
	FOnGraphIslandCreated OnIslandCreated;

	const TMap<FGraphVertexHandle, TObjectPtr<UGraphVertex>>& GetVertices() const { return Vertices; }
	const TMap<FGraphEdgeHandle, TObjectPtr<UGraphEdge>>& GetEdges() const { return Edges; }
	const TMap<FGraphIslandHandle, TObjectPtr<UGraphIsland>>& GetIslands() const { return Islands; }
	const FGraphProperties& GetProperties() const { return Properties; }

	UGraphVertex* GetSafeVertexFromHandle(const FGraphVertexHandle& Handle) const;
	UGraphEdge* GetSafeEdgeFromHandle(const FGraphEdgeHandle& Handle) const;
	UGraphIsland* GetSafeIslandFromHandle(const FGraphIslandHandle& Handle) const;

	GAMEPLAYGRAPH_API friend void operator<<(IGraphSerialization& Output, const UGraph& Graph);
	GAMEPLAYGRAPH_API friend void operator>>(const IGraphDeserialization& Input, UGraph& Graph);

protected:
	/** When we add multiple edges into the graph. This function will ensure that the interactions we make externally are kept to a minimum. */
	void MergeOrCreateIslands(const TArray<FGraphEdgeHandle>& InEdges);

	/** After a change, this function will remove the island if it's empty or will attempt to split it into two smaller islands. */
	void RemoveOrSplitIsland(TObjectPtr<UGraphIsland> Island);

	/** Used for bulk loading in vertices/edges/islands. */
	void ReserveVertices(int32 Delta);
	void ReserveEdges(int32 Delta);
	void ReserveIslands(int32 Delta);
private:
	UPROPERTY()
	TMap<FGraphVertexHandle, TObjectPtr<UGraphVertex>> Vertices;

	UPROPERTY()
	TMap<FGraphEdgeHandle, TObjectPtr<UGraphEdge>> Edges;

	UPROPERTY()
	TMap<FGraphIslandHandle, TObjectPtr<UGraphIsland>> Islands;
	FGraphProperties Properties;

	/** Creates an island out of a given set of nodes. */
	FGraphIslandHandle CreateIsland(TArray<FGraphVertexHandle> Nodes, FGraphUniqueIndex InUniqueIndex = FGraphUniqueIndex());

	/** Creates an edge between the two nodes. */
	FGraphEdgeHandle CreateEdge(FGraphVertexHandle Node1, FGraphVertexHandle Node2, FGraphUniqueIndex InUniqueIndex = FGraphUniqueIndex(), bool bMergeIslands = true);

	/** Adds a node to the graph's node collection and modifies the NextAvailableNodeUniqueIndex as necessary to maintain its validity. */
	void RegisterVertex(TObjectPtr<UGraphVertex> Node);

	/** Add an edge to the graph and modifies the NextAvailableEdgeUniqueIndex as necessary to maintain its validity. */
	void RegisterEdge(TObjectPtr<UGraphEdge> Edge);

	/** Add an island to the graph and modifies the NextAvailableIslandUniqueIndex as necessary to maintain its validity. */
	void RegisterIsland(TObjectPtr<UGraphIsland> Edge);

	/** Need this so that users aren't able to pass in bHandleIslands = false. */
	void RemoveEdgeInternal(const FGraphEdgeHandle& EdgeHandle, bool bHandleIslands);

	/** Factory functions for creating an appropriately typed node/edge/island. */
	virtual TObjectPtr<UGraphVertex> CreateTypedVertex() const;
	virtual TObjectPtr<UGraphEdge> CreateTypedEdge() const;
	virtual TObjectPtr<UGraphIsland> CreateTypedIsland() const;
};