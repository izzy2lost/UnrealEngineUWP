// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Scheduler/ScheduleHandle.h"
#include "Module/AnimNextModule.h"
#include "Graph/AnimNextGraphInstancePtr.h"
#include "Param/ParamStack.h"
#include "TraitCore/TraitEvent.h"
#include "TraitCore/TraitEventList.h"

class UAnimNextSchedule;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FParamStack;
	struct FParamStackLayerHandle;
	class IParameterSource;
	struct FPropertyBagProxy;
	enum class EParameterScopeOrdering : int32;
}

namespace UE::AnimNext
{

// Host for all data needed to run a schedule instance
struct FScheduleInstanceData : public FGCObject
{
	FScheduleInstanceData(const FScheduleContext& InScheduleContext, const UAnimNextSchedule* InSchedule, FScheduleHandle InHandle, FAnimNextSchedulerEntry* InCurrentEntry);

	~FScheduleInstanceData();
	
	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	// Apply the supplied parameter source to the specified scope, evicting any source that was there previously
	void ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, TUniquePtr<IParameterSource>&& InParameters);

	// Queues an input trait event
	// Input events will be processed in the next graph update after they are queued
	void QueueInputTraitEvent(FAnimNextTraitEventPtr Event);

	// Get the appropriate params stack given the ID
	TSharedPtr<FParamStack> GetParamStack(uint32 InIndex) const;
	
	// Handle to the currently executing entry in the schedule
	FScheduleHandle Handle;

	// Schedule entry that owns this instance
	FAnimNextSchedulerEntry* Entry = nullptr;

	// Scopes for user parameters to be applied at the root of the schedule
	TArray<TUniquePtr<IParameterSource>> RootUserScopes;

	// Pushed layers for the root scope
	TArray<FParamStack::FPushedLayerHandle> PushedRootUserLayers;

	struct FUserScope
	{
		// Layers that will be pushed before the scope, allowing the static scope to override the layer
		TMap<FName, TUniquePtr<IParameterSource>> BeforeSources;

		// Layers that will be pushed after the scope, overriding the static scope
		TMap<FName, TUniquePtr<IParameterSource>> AfterSources;
	};
	
	// Set of dynamic parameter scopes supplied by the user
	TMap<FName, FUserScope> UserScopes;

	// Cached data for each parameter scope
	struct FScopeCache
	{
		// Parameter sources at this scope
		TArray<TUniquePtr<IParameterSource>> ParameterSources;

		// Pushed layers, popped when scope exits
		TArray<FParamStack::FPushedLayerHandle> PushedLayers;
	};

	// Cached data for all param scopes
	TArray<FScopeCache> ScopeCaches;

	// Root param stack for the schedule itself (and globals)
	TSharedPtr<FParamStack> RootParamStack;

	// Param stacks required to run the schedule (one per task that requires a stack)
	TArray<TSharedPtr<FParamStack>> ParamStacks;

	// Intermediate data area
	FInstancedPropertyBag IntermediatesData;

	// Layer for intermediates data
	FParamStackLayerHandle IntermediatesLayer;

	// Remapped data layers for each port
	TArray<FParamStackLayerHandle> PortTermLayers;

	// Cached data for each graph
	struct FGraphCache
	{
		// Graph instance data
		FAnimNextGraphInstancePtr GraphInstanceData;

		// Remapped data layers for input terms (from schedule)
		FParamStackLayerHandle GraphTermLayer;
	};

	TArray<FGraphCache> GraphCaches;

	// Cached data for each external param task
	struct FExternalParamCache
	{
		// Parameter sources at this scope
		TArray<TUniquePtr<IParameterSource>> ParameterSources;
	};

	TArray<FExternalParamCache> ExternalParamCaches;

	// Input event list to be processed on the next update
	FTraitEventList InputEventList;

	// Output event list to be processed at the end of the schedule tick
	FTraitEventList OutputEventList;

	// Lock to ensure event list actions are thread safe
	FRWLock EventListLock;
};

}