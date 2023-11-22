// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleGraphTask.h"

#include "Scheduler/ScheduleContext.h"
#include "Graph/AnimNextGraph.h"
#include "Context.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "EvaluationVM/EvaluationVM.h"
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
	FParamStack::FPushedLayerHandle LayerHandle = ParamStack.PushLayer(InstanceData.GraphInputLayers[TaskIndex]);

	// Internally we use memstack allocation, so we need a mark here
	FMemStack& MemStack = FMemStack::Get();
	FMemMark MemMark(MemStack);

	// We allocate a dummy buffer to trigger the allocation of a large chunk if this is the first mark
	// This reduces churn internally by avoiding a chunk to be repeatedly allocated and freed as we push/pop marks
	MemStack.Alloc(size_t(FPageAllocator::SmallPageSize) + 1, 16);

	UE::AnimNext::UpdateGraph(InstanceData.GraphInstanceData[TaskIndex], InContext.GetDeltaTime());

	{
		const FEvaluationProgram EvaluationProgram = UE::AnimNext::EvaluateGraph(InstanceData.GraphInstanceData[TaskIndex]);

		FEvaluationVM EvaluationVM(EEvaluationFlags::All, *GraphReferencePose->ReferencePose, *GraphLODLevel);
		bool bHasValidOutput = false;

		if (!EvaluationProgram.IsEmpty())
		{
			EvaluationProgram.Execute(EvaluationVM);

			TUniquePtr<FKeyframeState> EvaluatedKeyframe;
			if (EvaluationVM.PopValue(KEYFRAME_STACK_NAME, EvaluatedKeyframe))
			{
				OutputPose->LODPose.CopyFrom(EvaluatedKeyframe->Pose);
				bHasValidOutput = true;
			}
		}

		if (!bHasValidOutput)
		{
			// We need to output a valid pose, generate one
			FKeyframeState ReferenceKeyframe = EvaluationVM.MakeReferenceKeyframe(false);
			OutputPose->LODPose.CopyFrom(ReferenceKeyframe.Pose);
		}
	}

	ParamStack.PopLayer(LayerHandle);
}
