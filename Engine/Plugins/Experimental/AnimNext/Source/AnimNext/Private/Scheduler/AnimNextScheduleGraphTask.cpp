// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleGraphTask.h"

#include "Scheduler/ScheduleContext.h"
#include "Graph/AnimNextGraph.h"
#include "Context.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Graph/AnimNext_LODPose.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_Task_Graph);

void FAnimNextScheduleGraphTask::RunGraph(const UE::AnimNext::FScheduleContext& InContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_Graph);

	using namespace UE::AnimNext;

	FParamStack& ParamStack = FParamStack::Get();

	UAnimNextGraph* GraphToRun = Graph;
	if (GraphToRun == nullptr && DynamicGraph != NAME_None)
	{
		if(const TObjectPtr<UAnimNextGraph>* FoundGraph = ParamStack.GetParamPtr<TObjectPtr<UAnimNextGraph>>(DynamicGraph))
		{
			GraphToRun = *FoundGraph;
		}
	}

	if(GraphToRun == nullptr)
	{
		return;
	}

	FScheduleInstanceData& InstanceData = InContext.GetInstanceData();

	// Check if we are running the correct graph and release (and any term mapping layers) it if not
	if(InstanceData.GraphInstanceData[TaskIndex].IsValid() && !InstanceData.GraphInstanceData[TaskIndex].UsesGraph(GraphToRun))
	{
		InstanceData.GraphInstanceData[TaskIndex].Release();
		InstanceData.GraphTermLayers[TaskIndex].Invalidate();
	}

	// Allocate our graph instance data
	if (!InstanceData.GraphInstanceData[TaskIndex].IsValid())
	{
		GraphToRun->AllocateInstance(InstanceData.GraphInstanceData[TaskIndex]);
	}

	const FAnimNextGraphReferencePose* GraphReferencePose = ParamStack.GetParamPtr<FAnimNextGraphReferencePose>(GraphToRun->GetReferencePoseParam());
	if(GraphReferencePose == nullptr)
	{
		return;
	}

	const int32* GraphLODLevel = ParamStack.GetParamPtr<int32>(GraphToRun->GetCurrentLODParam());
	if(GraphLODLevel == nullptr)
	{
		return;
	}

	// Check and allocate remapped term layer
	FParamStackLayerHandle& TermLayerHandle = InstanceData.GraphTermLayers[TaskIndex];
	if(!TermLayerHandle.IsValid())
	{
		TConstArrayView<FScheduleTerm> GraphTerms = GraphToRun->GetTerms();
		check(Terms.Num() == GraphTerms.Num());

		TMap<FName, FName> Mapping;
		Mapping.Reserve(GraphTerms.Num());
		for(int32 TermIndex = 0; TermIndex < Terms.Num(); ++TermIndex)
		{
			uint32 IntermediateTermIndex = Terms[TermIndex];
			const FPropertyBagPropertyDesc& PropertyDesc = InstanceData.IntermediatesData.GetPropertyBagStruct()->GetPropertyDescs()[IntermediateTermIndex];
			Mapping.Add(PropertyDesc.Name, GraphTerms[TermIndex].GetName());
		}

		TermLayerHandle = FParamStack::MakeRemappedLayer(InstanceData.IntermediatesLayer, Mapping);
	}

	// TODO: This should not be fixed at arg 0, we should define this in the graph asset
	FAnimNextGraphLODPose* OutputPose = TermLayerHandle.GetMutableParamPtr<FAnimNextGraphLODPose>(GraphToRun->GetTerms()[0].GetId());
	if(OutputPose == nullptr)
	{
		return;
	}

	// Create or update our result pose
	// TODO: Currently forcing additive flag to false here
	if (OutputPose->LODPose.ShouldPrepareForLOD(*GraphReferencePose->ReferencePose, *GraphLODLevel, false))
	{
		OutputPose->LODPose.PrepareForLOD(*GraphReferencePose->ReferencePose, *GraphLODLevel, true, false);
	}

	check(OutputPose->LODPose.LODLevel == *GraphLODLevel);

	// Push parameter layers that translate schedule data to graph inputs
	// TODO: This should probably be reworked, its overly complex for what it does!
	static FParamId ResultId("UE_Internal_ResultPose");
	static FParamId ExpectsAdditiveId("UE_Internal_GraphExpectsAdditive");

	if(!InstanceData.GraphInputLayers[TaskIndex].IsValid())
	{
		InstanceData.GraphInputLayers[TaskIndex] = FParamStack::MakeValuesLayer(
			ResultId, *OutputPose,
			ExpectsAdditiveId, false);
	}
	else
	{
		InstanceData.GraphInputLayers[TaskIndex].SetValues(
			ResultId, *OutputPose,
			ExpectsAdditiveId, false);
	}

	FParamStack::FPushedLayerHandle LayerHandle = ParamStack.PushLayer(InstanceData.GraphInputLayers[TaskIndex]);

	// Internally we use memstack allocation, so we need a mark here
	FMemStack& MemStack = FMemStack::Get();
	FMemMark MemMark(MemStack);

	// We allocate a dummy buffer to trigger the allocation of a large chunk if this is the first mark
	// This reduces churn internally by avoiding a chunk to be repeatedly allocated and freed as we push/pop marks
	MemStack.Alloc(size_t(FPageAllocator::SmallPageSize) + 1, 16);

	const float DeltaTime = InContext.GetDeltaTime();
	const FContext Context(DeltaTime);
	GraphToRun->Run(Context, InstanceData.GraphInstanceData[TaskIndex], EAnimNextGraphSimulationSteps::All);

	ParamStack.PopLayer(LayerHandle);
}
