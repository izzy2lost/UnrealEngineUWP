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

	if (Graph == nullptr)
	{
		return;
	}

	FScheduleInstanceData& InstanceData = InContext.GetInstanceData();

	// Allocate our graph instance data
	if (!InstanceData.GraphInstanceData[TaskIndex].IsValid())
	{
		Graph->AllocateInstance(InstanceData.GraphInstanceData[TaskIndex]);
	}

	FParamStack& ParamStack = FParamStack::Get();
	const FAnimNextGraphReferencePose* GraphReferencePose = ParamStack.GetParamPtr<FAnimNextGraphReferencePose>(Graph->GetReferencePoseParam());
	if(GraphReferencePose == nullptr)
	{
		return;
	}

	const int32* GraphLODLevel = ParamStack.GetParamPtr<int32>(Graph->GetCurrentLODParam());
	if(GraphLODLevel == nullptr)
	{
		return;
	}

	// TODO: This should not be fixed at arg 0, we should define this in the graph asset
	const FParamStackLayerHandle& TermLayerHandle = InstanceData.GraphTermLayers[TaskIndex];
	FAnimNextGraphLODPose* OutputPose = TermLayerHandle.GetMutableParamPtr<FAnimNextGraphLODPose>(Graph->GetTerms()[0].GetId());
	if(OutputPose == nullptr)
	{
		return;
	}

	float DeltaTime = 1/60.0f;
	const float* ExternalDeltaTime = ParamStack.GetParamPtr<float>(Graph->GetDeltaTimeParam());
	if(ExternalDeltaTime != nullptr)
	{
		DeltaTime = *ExternalDeltaTime;
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
	FMemMark MemMark(FMemStack::Get());

	const FContext Context(DeltaTime);
	Graph->Run(Context, InstanceData.GraphInstanceData[TaskIndex], EAnimNextGraphSimulationSteps::All);

	ParamStack.PopLayer(LayerHandle);
}
