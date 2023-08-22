// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "Graph/PCGStackContext.h"

class UPCGGraph;
struct FPCGGraphTask;

/** 
* FPCGGraphCompiler
* This class compiles a graph into tasks and keeps an internal cache
*/
class FPCGGraphCompiler
{
public:
	void Compile(UPCGGraph* InGraph);
	TArray<FPCGGraphTask> GetCompiledTasks(UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph = true);
	TArray<FPCGGraphTask> GetPrecompiledTasks(UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph = true) const;

	static void OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset, FPCGTaskId ParentId);

	/** Propagates grid sizes through a graph's compiled tasks. */
	void ResolveGridSizes(TArray<FPCGGraphTask>& InOutCompiledTasks, const FPCGStackContext& InStackContext, EPCGHiGenGrid GenerationDefaultGrid) const;

#if WITH_EDITOR
	void NotifyGraphChanged(UPCGGraph* InGraph);
#endif

	/** Flush all cached compiled graphs. */
	void ClearCache();

private:
	TArray<FPCGGraphTask> CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId, FPCGStackContext& InOutStackContext);
	void CompileTopGraph(UPCGGraph* InGraph, uint32 GenerationGridSize);

	/** Returns the grid for InCompiledTaskId. */
	EPCGHiGenGrid CalculateGridRecursive(FPCGTaskId InTaskId, EPCGHiGenGrid GenerationDefaultGrid, const FPCGStackContext& InStackContext, TArray<FPCGGraphTask>& InOutCompiledTasks) const;

	mutable FRWLock GraphToTaskMapLock;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> GraphToTaskMap;
	TMap<UPCGGraph*, FPCGStackContext> GraphToStackContext;
	// Top graphs are optimized for execution grid and store one set of compiled tasks per grid size.
	TMap<UPCGGraph*, TMap<uint32, TArray<FPCGGraphTask>>> TopGraphToTaskMap;
	TMap<UPCGGraph*, FPCGStackContext> TopGraphToStackContext;

#if WITH_EDITOR
	void RemoveFromCache(UPCGGraph* InGraph);
	void RemoveFromCacheRecursive(UPCGGraph* InGraph);

	FCriticalSection GraphDependenciesLock;
	TMultiMap<UPCGGraph*, UPCGGraph*> GraphDependencies;
#endif // WITH_EDITOR
};
