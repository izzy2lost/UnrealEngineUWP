// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleGraphTask.h"

#include "IAnimNextModule.h"
#include "Scheduler/ScheduleContext.h"
#include "Graph/AnimNextGraph.h"
#include "Context.h"
#include "Graph/AnimNext_LODPose.h"
#include "AnimNextStats.h"
#include "Component/AnimNextComponent.h"
#include "Component/AnimNextMeshComponent.h"
#include "GameFramework/Character.h"
#include "Logging/StructuredLog.h"
#include "Param/AnimNextObjectCastLocatorFragment.h"
#include "Param/AnimNextObjectFunctionLocatorFragment.h"
#include "Param/AnimNextParam.h"
#include "Param/AnimNextParamUniversalObjectLocator.h"

DEFINE_STAT(STAT_AnimNext_Task_Graph);

namespace UE::AnimNext
{
	// TODO: Currently we hard-code the ACharacter mesh component as the source of the reference pose and LOD index, but this should be a pin-input in the
	// final schedule incarnation.
	TInstancedStruct<FAnimNextParamUniversalObjectLocator> GetCharacterInstanceId()
	{
		TInstancedStruct<FAnimNextParamUniversalObjectLocator> Locator = TInstancedStruct<FAnimNextParamUniversalObjectLocator>::Make();
		Locator.GetMutable().Locator.AddFragment<FAnimNextObjectFunctionLocatorFragment>(UAnimNextComponent::StaticClass()->FindFunctionByName("GetOwner"));
		Locator.GetMutable().Locator.AddFragment<FAnimNextObjectCastLocatorFragment>(ACharacter::StaticClass());
		return Locator;
	};

	FName GetCharacterInstanceIdName()
	{
		static FName Name(NAME_None);
		if(Name.IsNone())
		{
			const TInstancedStruct<FAnimNextParamUniversalObjectLocator>& Locator = GetCharacterInstanceId();
			Name = Locator.Get().ToName();
		}
		return Name;
	};

	FParamId GetMeshComponentParamId()
	{
		static const FParamId MeshComponentParamId("/Script/Engine.Character:Mesh", GetCharacterInstanceIdName());
		return MeshComponentParamId;
	}
}

UAnimNextGraph* FAnimNextScheduleGraphTask::GetGraphToRun(UE::AnimNext::FParamStack& ParamStack) const
{
	UAnimNextGraph* GraphToRun = Graph;
	if (GraphToRun == nullptr && DynamicGraph.IsValid())
	{
		if(const TObjectPtr<UAnimNextGraph>* FoundGraph = ParamStack.GetParamPtr<TObjectPtr<UAnimNextGraph>>(DynamicGraph.GetParamId()))
		{
			GraphToRun = *FoundGraph;
		}
	}

	return GraphToRun;
}

void FAnimNextScheduleGraphTask::VerifyRequiredParameters(UAnimNextGraph* InGraphToRun) const
{
	if(SuppliedParametersHash != InGraphToRun->RequiredParametersHash)
	{
		bool bWarningOutput = false;

		for(const FAnimNextParam& RequiredParameter : InGraphToRun->RequiredParameters)
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
				UE_LOGFMT(LogAnimation, Warning, "AnimNext: Graph {GraphToRun} has different required parameters, it may not run correctly.", InGraphToRun->GetFName());
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

void FAnimNextScheduleGraphTask::RunGraph(const UE::AnimNext::FScheduleContext& InContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_Graph);

	using namespace UE::AnimNext;

	FParamStack& ParamStack = FParamStack::Get();

	UAnimNextGraph* GraphToRun = GetGraphToRun(ParamStack);
	if(GraphToRun == nullptr)
	{
		return;
	}

	FScheduleInstanceData& InstanceData = InContext.GetInstanceData();
	FScheduleInstanceData::FGraphCache& GraphCache = InstanceData.GraphCaches[TaskIndex];

	// Check if we are running the correct graph and release (and any term mapping layers) it if not
	if(GraphCache.GraphInstanceData.IsValid() && !GraphCache.GraphInstanceData.UsesGraph(GraphToRun))
	{
		GraphCache.GraphInstanceData.Release();
		GraphCache.GraphTermLayer.Invalidate();
	}

	// Allocate our graph instance data
	if (!GraphCache.GraphInstanceData.IsValid())
	{
		GraphToRun->AllocateInstance(GraphCache.GraphInstanceData, EntryPoint.Name);

		// Only do dynamic verification for dynamic graphs. Static graphs get verified at compile time. 
		if (Graph == nullptr && DynamicGraph.IsValid())
		{
			VerifyRequiredParameters(GraphToRun);
		}
	}

	const TObjectPtr<USkeletalMeshComponent>* ComponentPtr = ParamStack.GetParamPtr<TObjectPtr<USkeletalMeshComponent>>(GetMeshComponentParamId());
	if(ComponentPtr == nullptr)
	{
		return;
	}

	TObjectPtr<UAnimNextMeshComponent> Component = Cast<UAnimNextMeshComponent>(*ComponentPtr);
	if(Component == nullptr)
	{
		return;
	}
	
	const FAnimNextGraphReferencePose GraphReferencePose = Component->GetReferencePose();
	if(!GraphReferencePose.ReferencePose.IsValid())
	{
		return;
	}

	// Check and allocate remapped term layer
	if(!GraphCache.GraphTermLayer.IsValid())
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

		GraphCache.GraphTermLayer = FParamStack::MakeRemappedLayer(InstanceData.IntermediatesLayer, Mapping);
	}

	// TODO: This should not be fixed at arg 0, we should define this in the graph asset
	FAnimNextGraphLODPose* OutputPose = GraphCache.GraphTermLayer.GetMutableParamPtr<FAnimNextGraphLODPose>(GraphToRun->GetTerms()[0].GetId());
	if(OutputPose == nullptr)
	{
		return;
	}
	
	const UE::AnimNext::FReferencePose& RefPose = GraphReferencePose.ReferencePose.GetRef<UE::AnimNext::FReferencePose>();

	const int32 LODIndex = Component->GetPredictedLODLevel();

	// Create or update our result pose
	// TODO: Currently forcing additive flag to false here
	if (OutputPose->LODPose.ShouldPrepareForLOD(RefPose, LODIndex, false))
	{
		OutputPose->LODPose.PrepareForLOD(RefPose, LODIndex, true, false);
	}

	check(OutputPose->LODPose.LODLevel == LODIndex);

	// Internally we use memstack allocation, so we need a mark here
	FMemStack& MemStack = FMemStack::Get();
	FMemMark MemMark(MemStack);

	// We allocate a dummy buffer to trigger the allocation of a large chunk if this is the first mark
	// This reduces churn internally by avoiding a chunk to be repeatedly allocated and freed as we push/pop marks
	MemStack.Alloc(size_t(FPageAllocator::SmallPageSize) + 1, 16);

	IAnimNextModule::Get().UpdateGraph(GraphCache.GraphInstanceData, InContext.GetDeltaTime());
	IAnimNextModule::Get().EvaluateGraph(GraphCache.GraphInstanceData, RefPose, LODIndex, OutputPose->LODPose);
}

#if WITH_EDITORONLY_DATA 

TArray<FAnimNextEditorParam> FAnimNextScheduleGraphTask::GetRequiredParametersInternal()
{
	using namespace UE::AnimNext;

	const TArray<FAnimNextEditorParam> Params =
	{
		FAnimNextEditorParam(GetMeshComponentParamId().GetName(), FAnimNextParamType::GetType<TObjectPtr<UAnimNextMeshComponent>>(), GetCharacterInstanceId()),
	};

	return Params;
}

#endif