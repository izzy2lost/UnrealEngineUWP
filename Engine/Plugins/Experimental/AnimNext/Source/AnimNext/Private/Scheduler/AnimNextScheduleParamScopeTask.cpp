// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleParamScopeTask.h"
#include "Param/AnimNextParameterBlock.h"
#include "Scheduler/ScheduleContext.h"
#include "Scheduler/AnimNextSchedulerEntry.h"
#include "Param/AnimNextParameterSourceRef.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_Task_ScopeEntry);
DEFINE_STAT(STAT_AnimNext_Task_ScopeExit);

void FAnimNextScheduleParamScopeEntryTask::RunParamScopeEntry(const UE::AnimNext::FScheduleContext& InScheduleContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_ScopeEntry);

	using namespace UE::AnimNext;

	FParamStack& ParamStack = FParamStack::Get();

	FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
	FScheduleInstanceData::FScopeCache& ScopeCache = InstanceData.ScopeCaches[TaskIndex];
	ScopeCache.PushedLayers.Reset();

	const FAnimNextParameterCollection* FoundUserScope = InstanceData.UserScopes.Find(Scope);
	if(FoundUserScope)
	{
		ScopeCache.PushedLayers.Reserve(ParameterBlocks.Num() + FoundUserScope->Parameters.Num());
	}
	else
	{
		ScopeCache.PushedLayers.Reserve(ParameterBlocks.Num());
	}

	// Resize layer handles appropriately & cache layer
	if (ScopeCache.StaticHandles.Num() != ParameterBlocks.Num())
	{
		ScopeCache.StaticHandles.SetNum(ParameterBlocks.Num());
		for (int32 LayerHandleIndex = 0; LayerHandleIndex < ScopeCache.StaticHandles.Num(); ++LayerHandleIndex)
		{
			if(ParameterBlocks[LayerHandleIndex])
			{
				ScopeCache.StaticHandles[LayerHandleIndex] = ParameterBlocks[LayerHandleIndex]->CacheLayer();
			}
		}
	}

	if (FoundUserScope)
	{
		const UObject* ObjectContext = InstanceData.Entry->ResolvedObject;

		// Resize user scope data appropriately
		if (ScopeCache.UserHandles.Num() != FoundUserScope->Parameters.Num())
		{
			ScopeCache.UserHandles.SetNum(FoundUserScope->Parameters.Num());
		}

		// Cache any layers if required
		for (int32 LayerHandleIndex = 0; LayerHandleIndex < ScopeCache.UserHandles.Num(); ++LayerHandleIndex)
		{
			if(const IAnimNextParameterSourceInterface* ParameterSource = FoundUserScope->Parameters[LayerHandleIndex].Get(ObjectContext))
			{
				if(ParameterSource->ShouldCacheLayer(ScopeCache.UserHandles[LayerHandleIndex]))
				{
					ScopeCache.UserHandles[LayerHandleIndex] = ParameterSource->CacheLayer();
				}
			}
		}

		for (int32 ParamBlockIndex = 0; ParamBlockIndex < FoundUserScope->Parameters.Num(); ++ParamBlockIndex)
		{
			if(const IAnimNextParameterSourceInterface* ParameterSource = FoundUserScope->Parameters[ParamBlockIndex].Get(ObjectContext))
			{
				FParamStackLayerHandle& LayerHandle = ScopeCache.UserHandles[ParamBlockIndex];
				ParameterSource->UpdateLayer(LayerHandle);
				ScopeCache.PushedLayers.Add(ParamStack.PushLayer(LayerHandle));
			}
		}
	}

	// TODO: Pre/post scope support

	// Update & push static params
	for (int32 ParamBlockIndex = 0; ParamBlockIndex < ParameterBlocks.Num(); ++ParamBlockIndex)
	{
		if(const UAnimNextParameterBlock* ParameterBlock = ParameterBlocks[ParamBlockIndex])
		{
			FParamStackLayerHandle& LayerHandle = ScopeCache.StaticHandles[ParamBlockIndex];

			ParameterBlock->UpdateLayer(LayerHandle);
			ScopeCache.PushedLayers.Add(ParamStack.PushLayer(LayerHandle));
		}
	}
}

void FAnimNextScheduleParamScopeExitTask::RunParamScopeExit(const UE::AnimNext::FScheduleContext& InScheduleContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_ScopeExit);

	using namespace UE::AnimNext;

	FParamStack& ParamStack = FParamStack::Get();

	FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
	FScheduleInstanceData::FScopeCache& ScopeCache = InstanceData.ScopeCaches[TaskIndex];
	for (int32 LayerIndex = ScopeCache.PushedLayers.Num() - 1; LayerIndex >= 0; --LayerIndex)
	{
		ParamStack.PopLayer(ScopeCache.PushedLayers[LayerIndex]);
	}

	ScopeCache.PushedLayers.Reset();
}