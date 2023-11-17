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
	TArray<FPCGGraphTask> GetPrecompiledTasks(const UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph = true) const;

	static void OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset, FPCGTaskId ParentId);

#if WITH_EDITOR
	void NotifyGraphChanged(UPCGGraph* InGraph);
#endif

	/** Flush all cached compiled graphs. */
	void ClearCache();

private:
	TArray<FPCGGraphTask> CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId, FPCGStackContext& InOutStackContext);
	void CompileTopGraph(UPCGGraph* InGraph, uint32 GenerationGridSize);

	/** Propagates grid sizes through a graph's compiled tasks. */
	static void ResolveGridSizes(
		EPCGHiGenGrid GenerationGrid,
		const TArray<FPCGGraphTask>& CompiledTasks,
		const FPCGStackContext& StackContext,
		EPCGHiGenGrid GenerationDefaultGrid,
		TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid);

	/** Returns the execution grid for the given task. */
	static EPCGHiGenGrid CalculateGridRecursive(
		FPCGTaskId InTaskId,
		EPCGHiGenGrid GenerationDefaultGrid,
		const FPCGStackContext& InStackContext,
		const TArray<FPCGGraphTask>& InCompiledTasks,
		TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid);

	/** Create linkage tasks for edges that cross from large grid to small grid tasks. */
	static void CreateGridLinkages(
		EPCGHiGenGrid InGenerationGrid,
		TArray<EPCGHiGenGrid>& TaskGenerationGrid,
		TArray<FPCGGraphTask>& InOutCompiledTasks,
		const FPCGStackContext& InStackContext);

	/** Culls tasks based on a given lambda. Never culls the first (input) task in the array. */
	static void CullTasks(TArray<FPCGGraphTask>& InOutCompiledTasks, bool bAddPassthroughWires, TFunctionRef<bool(const FPCGGraphTask&)> CullTask);

	/** Remove any stack frames that are not used by any task. */
	static void PostCullStackCleanup(TArray<FPCGGraphTask>& InCompiledTasks, FPCGStackContext& InOutStackContext);

	mutable FRWLock GraphToTaskMapLock;
	TMap<UPCGGraph*, TArray<FPCGGraphTask>> GraphToTaskMap;
	TMap<UPCGGraph*, FPCGStackContext> GraphToStackContext;

	// Top graphs are optimized for execution grid and store one set of compiled tasks per grid size.
	TMap<UPCGGraph*, TMap<uint32, TArray<FPCGGraphTask>>> TopGraphToTaskMap;
	TMap<UPCGGraph*, TMap<uint32, FPCGStackContext>> TopGraphToStackContextMap;

#if WITH_EDITOR
	void RemoveFromCache(UPCGGraph* InGraph);
	void RemoveFromCacheRecursive(UPCGGraph* InGraph);

	FCriticalSection GraphDependenciesLock;
	TMultiMap<UPCGGraph*, UPCGGraph*> GraphDependencies;
#endif // WITH_EDITOR
};
