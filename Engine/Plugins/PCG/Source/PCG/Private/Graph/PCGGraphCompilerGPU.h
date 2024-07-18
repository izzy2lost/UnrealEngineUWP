// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"

class UPCGGraph;
class UPCGPin;
struct FPCGGraphTask;

class FPCGGraphCompilerGPU
{
public:
	/** Used to track new unique virtual pins created on generated compute graph elements. */
	using FNodePin = TTuple<FPCGTaskId, /*Pin label*/FName, /*Pin is input*/bool>;
	using FOriginalToVirtualPin = TMap<FNodePin, /*Virtual pin label*/FName>;
	using FTaskToSuccessors = TMap<FPCGTaskId, TArray<FPCGTaskId>>;

	/** Identifies connected sets of GPU nodes, giving each a non - zero ID value. */
	static void LabelConnectedGPUNodeIslands(
		const TArray<FPCGGraphTask>& InCompiledTasks,
		const TSet<FPCGTaskId>& InGPUCompatibleTaskIds,
		const FTaskToSuccessors& InTaskSuccessors,
		TArray<uint32>& OutIslandIDs);

	/** Outputs sets of task IDs, where each set is GPU nodes that can be compiled into a compute graph and dispatched together. */
	static void CollectGPUNodeSubsets(
		const TArray<FPCGGraphTask>& InCompiledTasks,
		const FTaskToSuccessors& InTaskSuccessors,
		const TSet<FPCGTaskId>& InGPUCompatibleTaskIds,
		TArray<TSet<FPCGTaskId>>& OutNodeSubsetsToConvertToCFGraph);
	
	/** For GPU node inputs that have multiple incident edges, bundle them into a single edge. This is to avoid an inefficient
	* gather operation on the GPU, and allows data interfaces to pick their data from the compute graph element input data collection
	* using unique virtual input pin labels. */
	static void CreateGatherTasksAtGPUInputs(const TSet<FPCGTaskId>& InGPUCompatibleTaskIds, TArray<FPCGGraphTask>& InOutCompiledTasks);
	
	/** Wires in a compute graph element alongside each set of GPU compatible nodes. The tasks for each node will be culled later. */
	static void WireGPUGraphNode(
		FPCGTaskId InGPUGraphTaskId,
		const TSet<FPCGTaskId>& InCollapsedTasks,
		const TSet<FPCGTaskId>& InGPUCompatibleTaskIds,
		TArray<FPCGGraphTask>& InOutCompiledTasks,
		const TMap<FPCGTaskId,
		TArray<FPCGTaskId>>&InTaskSuccessors,
		FOriginalToVirtualPin& OutOriginalToVirtualPin,
		TMap<TObjectPtr<const UPCGPin>, FName>& OutOutputCPUPinToVirtualPin);
	
	/** Compiles a compute graph. */
	static void BuildGPUGraphTask(
		UPCGGraph* InGraph,
		FPCGTaskId InGPUGraphTaskId,
		const TSet<FPCGTaskId>& InCollapsedTasks,
		const FTaskToSuccessors& InTaskSuccessors,
		TArray<FPCGGraphTask>& InOutCompiledTasks,
		const FOriginalToVirtualPin& InOriginalToVirtualPin,
		const TMap<TObjectPtr<const UPCGPin>, FName>& InOutputCPUPinToVirtualPin);
	
	/** Finds connected subgraphs of GPU - enabled nodes that can be dispatched together and replaces each one with a compute graph. */
	static void CreateGPUNodes(UPCGGraph* InGraph, TArray<FPCGGraphTask>& InOutCompiledTasks);
};
