// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleGraphTask.h"

#include "IAnimNextModuleInterface.h"
#include "Scheduler/ScheduleContext.h"
#include "Module/AnimNextModule.h"
#include "Context.h"
#include "Graph/AnimNext_LODPose.h"
#include "AnimNextStats.h"
#include "Component/AnimNextComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Logging/StructuredLog.h"
#include "Param/AnimNextParam.h"
#include "Param/AnimNextParamUniversalObjectLocator.h"
#include "Param/ParametersProxy.h"

DEFINE_STAT(STAT_AnimNext_Task_Graph);

UAnimNextModule* FAnimNextScheduleGraphTask::GetModuleToRun(UE::AnimNext::FParamStack& ParamStack) const
{
	UAnimNextModule* ModuleToRun = Module;
	if (ModuleToRun == nullptr && DynamicModule.IsValid())
	{
		if(const TObjectPtr<UAnimNextModule>* FoundModule = ParamStack.GetParamPtr<TObjectPtr<UAnimNextModule>>(DynamicModule.GetParamId()))
		{
			ModuleToRun = *FoundModule;
		}
	}

	return ModuleToRun;
}

void FAnimNextScheduleGraphTask::VerifyRequiredParameters(UAnimNextModule* InModuleToRun) const
{
	if(SuppliedParametersHash != InModuleToRun->RequiredParametersHash)
	{
		bool bWarningOutput = false;

		for(const FAnimNextParam& RequiredParameter : InModuleToRun->RequiredParameters)
		{
			bool bFound = false;
			bool bFoundCorrectType = true;
			FAnimNextParamType SuppliedParameterType;
			for(const FAnimNextParam& SuppliedParameter : SuppliedParameters)
			{
				if(RequiredParameter.Name == SuppliedParameter.Name && RequiredParameter.InstanceId == SuppliedParameter.InstanceId)
				{
					if(RequiredParameter.Type != SuppliedParameter.Type)
					{
						SuppliedParameterType = SuppliedParameter.Type;
						bFoundCorrectType = false;
					}
					bFound = true;
					break;
				}
			}

			if(!bWarningOutput && (!bFound || !bFoundCorrectType))
			{
				UE_LOGFMT(LogAnimation, Warning, "AnimNext: Graph {ModuleToRun} has different required parameters, it may not run correctly.", InModuleToRun->GetFName());
				bWarningOutput = true;
			}
			
			if(!bFound)
			{
				UE_LOGFMT(LogAnimation, Warning, "    Not Found: {Name} (Instance: {Instance}))", RequiredParameter.Name, RequiredParameter.InstanceId);
			}
			else if(!bFoundCorrectType)
			{
				UE_LOGFMT(LogAnimation, Warning, "    Incorrect Type: {Name} ({RequiredType} vs {SuppliedType})", RequiredParameter.Name, RequiredParameter.Type.ToString(), SuppliedParameterType.ToString());
			}
		}
	}
}

void FAnimNextScheduleGraphTask::RunModule(const UE::AnimNext::FScheduleContext& InContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_Graph);

	using namespace UE::AnimNext;

	FParamStack& ParamStack = FParamStack::Get();

	UAnimNextModule* ModuleToRun = GetModuleToRun(ParamStack);
	if(ModuleToRun == nullptr)
	{
		return;
	}

	FScheduleInstanceData& InstanceData = InContext.GetInstanceData();
	FScheduleInstanceData::FGraphCache& GraphCache = InstanceData.GraphCaches[TaskIndex];

	// Check if we are running the correct graph and release (and any term mapping layers) it if not
	if(GraphCache.GraphInstanceData.IsValid() && !GraphCache.GraphInstanceData.UsesModule(ModuleToRun))
	{
		GraphCache.GraphInstanceData.Release();
		GraphCache.GraphTermLayer.Invalidate();
	}

	// Allocate our graph instance data
	if (!GraphCache.GraphInstanceData.IsValid())
	{
		ModuleToRun->AllocateInstance(GraphCache.GraphInstanceData, EntryPoint.Name);

		// Only do dynamic verification for dynamic graphs. Static graphs get verified at compile time. 
		if (Module == nullptr && DynamicModule.IsValid())
		{
			VerifyRequiredParameters(ModuleToRun);
		}
	}

	const FAnimNextGraphReferencePose* GraphRefPosePtr = ParamStack.GetParamPtr<FAnimNextGraphReferencePose>(ReferencePose.GetParamId());
	if (GraphRefPosePtr == nullptr || !GraphRefPosePtr->ReferencePose.IsValid())
	{
		return;
	}

	int32 LODIndex = 0;
	const int32* LODPtr = ParamStack.GetParamPtr<int32>(LOD.GetParamId());
	if (LODPtr != nullptr)
	{
		LODIndex = *LODPtr;
	}

	// Check and allocate remapped term layer
	if(!GraphCache.GraphTermLayer.IsValid())
	{
		TConstArrayView<FScheduleTerm> GraphTerms = ModuleToRun->GetTerms();
		check(Terms.Num() == GraphTerms.Num());

		TMap<FName, FName> Mapping;
		Mapping.Reserve(GraphTerms.Num());
		for(int32 TermIndex = 0; TermIndex < Terms.Num(); ++TermIndex)
		{
			uint32 IntermediateTermIndex = Terms[TermIndex];
			const FPropertyBagPropertyDesc& PropertyDesc = InstanceData.IntermediatesData.GetPropertyBagStruct()->GetPropertyDescs()[IntermediateTermIndex];
			Mapping.Add(PropertyDesc.Name, GraphTerms[TermIndex].GetName());
		}

		GraphCache.GraphTermLayer = FParamStack::MakeRemappedLayer(InstanceData.IntermediatesLayer, Mapping);
	}

	// TODO: This should not be fixed at arg 0, we should define this in the graph asset
	FAnimNextGraphLODPose* OutputPose = GraphCache.GraphTermLayer.GetMutableParamPtr<FAnimNextGraphLODPose>(ModuleToRun->GetTerms()[0].GetId());
	if(OutputPose == nullptr)
	{
		return;
	}

	const UE::AnimNext::FReferencePose& RefPose = GraphRefPosePtr->ReferencePose.GetRef<UE::AnimNext::FReferencePose>();

	// Create or update our result pose
	// TODO: Currently forcing additive flag to false here
	if (OutputPose->LODPose.ShouldPrepareForLOD(RefPose, LODIndex, false))
	{
		OutputPose->LODPose.PrepareForLOD(RefPose, LODIndex, true, false);
	}

	check(OutputPose->LODPose.LODLevel == LODIndex);

	// Every graph in a schedule will see the same input events (if they were queued before the schedule started)
	FTraitEventList InputEventList;
	FTraitEventList OutputEventList;

	// A schedule can contain multiple graphs, we copy the input event list since it might be appended to during our update
	{
		FReadScopeLock ReadLock(InstanceData.EventListLock);
		InputEventList = InstanceData.InputEventList;
	}

	// Track how many input events we started with, we'll append the new ones
	const int32 NumOriginalInputEvents = InputEventList.Num();

	// Internally we use memstack allocation, so we need a mark here
	FMemStack& MemStack = FMemStack::Get();
	FMemMark MemMark(MemStack);

	// We allocate a dummy buffer to trigger the allocation of a large chunk if this is the first mark
	// This reduces churn internally by avoiding a chunk to be repeatedly allocated and freed as we push/pop marks
	MemStack.Alloc(size_t(FPageAllocator::SmallPageSize) + 1, 16);

	IAnimNextModuleInterface& AnimNextModule = IAnimNextModuleInterface::Get();
	AnimNextModule.UpdateGraph(GraphCache.GraphInstanceData, InContext.GetDeltaTime(), InputEventList, OutputEventList);
	AnimNextModule.EvaluateGraph(GraphCache.GraphInstanceData, RefPose, LODIndex, OutputPose->LODPose);

	// We might have appended new input/output events, append them
	{
		const int32 NumInputEvents = InputEventList.Num();

		FWriteScopeLock WriteLock(InstanceData.EventListLock);

		// Append the new input events
		for (int32 EventIndex = NumOriginalInputEvents; EventIndex < NumInputEvents; ++EventIndex)
		{
			FAnimNextTraitEventPtr& Event = InputEventList[EventIndex];
			if (Event->IsValid())
			{
				InstanceData.InputEventList.Push(MoveTemp(Event));
			}
		}

		// Append our output events
		InstanceData.OutputEventList.Append(OutputEventList);
	}
}
