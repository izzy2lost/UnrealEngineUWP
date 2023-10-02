// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScheduleHandle.h"
#include "Param/AnimNextParameterCollection.h"
#include "Graph/AnimNextGraph.h"

struct FAnimNextGraphInstance;
class UAnimNextSchedule;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FParamStack;
	struct FParamStackLayerHandle;
}

namespace UE::AnimNext
{

// Host for all data needed to run a schedule instance
struct FScheduleInstanceData : public FGCObject
{
	FScheduleInstanceData(const FScheduleContext& InScheduleContext, const UAnimNextSchedule* InSchedule, FScheduleHandle InHandle, FAnimNextSchedulerEntry* InCurrentEntry, TMap<FName, FAnimNextParameterCollection>&& InDynamicScopes);

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	// Handle to the currently executing entry in the schedule
	FScheduleHandle Handle;

	// Schedule entry that owns this instance
	FAnimNextSchedulerEntry* Entry = nullptr;

	// Set of dynamic parameter scopes supplied by the user
	TMap<FName, FAnimNextParameterCollection> UserScopes;

	// Cached data for each parameter scope
	struct FScopeCache
	{
		// Cached handles for scheduled parameter blocks
		TArray<FParamStackLayerHandle> StaticHandles;

		// Cached handles for user-defined parameter blocks hooked to a scope
		TArray<FParamStackLayerHandle> UserHandles;

		// Pushed layers, popped when scope exits
		TArray<FParamStack::FPushedLayerHandle> PushedLayers;
	};

	// Cached data for all param scopes
	TArray<FScopeCache> ScopeCaches;

	// Root param stack for the schedule itself (and globals)
	TSharedPtr<FParamStack> RootParamStack;

	// Param stacks required to run the schedule (one per task that requires a stack)
	TArray<TSharedPtr<FParamStack>> ParamStacks;

	// Graph instance data for each graph task
	TArray<FAnimNextGraphInstance> GraphInstanceData;
};

}