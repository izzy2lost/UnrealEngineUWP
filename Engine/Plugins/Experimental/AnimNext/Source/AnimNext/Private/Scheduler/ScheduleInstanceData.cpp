// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/ScheduleHandle.h"
#include "Scheduler/AnimNextSchedulerWorldSubsystem.h"
#include "Scheduler/AnimNextSchedulerEntry.h"
#include "AnimNextStats.h"
#include "AnimNextModuleImpl.h"
#include "Modules/ModuleManager.h"
#include "Param/IParameterSourceFactory.h"
#include "Param/ParametersProxy.h"
#include "Scheduler/AnimNextScheduleExternalParamTask.h"
#include "Scheduler/ScheduleInitializationContext.h"

DEFINE_STAT(STAT_AnimNext_CreateInstanceData);

namespace UE::AnimNext
{

FScheduleInstanceData::FScheduleInstanceData(const FScheduleContext& InScheduleContext, const UAnimNextSchedule* InSchedule, FScheduleHandle InHandle, FAnimNextSchedulerEntry* InCurrentEntry)
	: Handle(InHandle)
	, Entry(InCurrentEntry) 
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_CreateInstanceData);

	// Preallocate data for all scopes & graphs in the schedule
	ScopeCaches.SetNum(InSchedule->NumParameterScopes);
	GraphCaches.SetNum(InSchedule->GraphTasks.Num());
	ExternalParamCaches.SetNum(InSchedule->ExternalParamTasks.Num());

	for(int32 EntryIndex = 0; EntryIndex < InSchedule->ParamScopeEntryTasks.Num(); ++EntryIndex)
	{
		const FAnimNextScheduleParamScopeEntryTask& EntryTask = InSchedule->ParamScopeEntryTasks[EntryIndex];
		FScopeCache& ScopeCache = ScopeCaches[EntryTask.ParamScopeIndex];
		ScopeCache.ParameterSources.Reserve(EntryTask.Parameters.Num());
		for(UAnimNextModule* Module : EntryTask.Parameters)
		{
			if(Module)
			{
				ScopeCache.ParameterSources.Emplace(MakeUnique<FParametersProxy>(Module));
			}
		}

		ScopeCache.PushedLayers.Reserve(ScopeCache.ParameterSources.Num() + 1); // +1 for any user handles added dynamically
	}

	// Setup param stack graph
	RootParamStack = InCurrentEntry->RootParamStack;
	ParamStacks.SetNum(InSchedule->NumParameterScopes);
	for (TSharedPtr<FParamStack>& ParamStack : ParamStacks)
	{
		ParamStack = MakeShared<FParamStack>();
	}

	for (const FAnimNextScheduleGraphTask& Task : InSchedule->GraphTasks)
	{
		const TSharedPtr<FParamStack> ParentStack = Task.ParamParentScopeIndex != MAX_uint32 ? ParamStacks[Task.ParamParentScopeIndex] : RootParamStack;
		ParamStacks[Task.ParamScopeIndex]->SetParent(ParentStack);
	}

	for (const FAnimNextScheduleExternalTask& ExternalTask : InSchedule->ExternalTasks)
	{
		const TSharedPtr<FParamStack> ParentStack = ExternalTask.ParamParentScopeIndex != MAX_uint32 ? ParamStacks[ExternalTask.ParamParentScopeIndex] : RootParamStack;
		ParamStacks[ExternalTask.ParamScopeIndex]->SetParent(ParentStack);
	}

	for (const FAnimNextScheduleParamScopeEntryTask& ScopeEntryTask : InSchedule->ParamScopeEntryTasks)
	{
		const TSharedPtr<FParamStack> ParentStack = ScopeEntryTask.ParamParentScopeIndex != MAX_uint32 ? ParamStacks[ScopeEntryTask.ParamParentScopeIndex] : RootParamStack;
		ParamStacks[ScopeEntryTask.ParamScopeIndex]->SetParent(ParentStack);
	}

	// Set up external parameters
	UE::AnimNext::FAnimNextModuleImpl& AnimNextModule = FModuleManager::GetModuleChecked<UE::AnimNext::FAnimNextModuleImpl>("AnimNext");

	FParameterSourceContext ParameterSourceContext;
	ParameterSourceContext.Object = Entry->WeakObject.Get();

	const float DeltaTime = InScheduleContext.GetDeltaTime();
	for(int32 ExternalParamSourceIndex = 0; ExternalParamSourceIndex < ExternalParamCaches.Num(); ++ExternalParamSourceIndex)
	{
		FExternalParamCache& ExternalParamCache = ExternalParamCaches[ExternalParamSourceIndex];

		for(const FAnimNextScheduleExternalParameterSource& ParameterSource : InSchedule->ExternalParamTasks[ExternalParamSourceIndex].ParameterSources)
		{
			if(TUniquePtr<IParameterSource> NewParameterSource = AnimNextModule.CreateParameterSource(ParameterSourceContext, ParameterSource.InstanceId, ParameterSource.Parameters))
			{
				// Initial update is required to populate the cache
				// TODO: This needs to move outside this function once we run initialization off the game thread, depending on thread-safety
				NewParameterSource->Update(DeltaTime);

				// External parameter layer is always pushed
				RootParamStack->PushLayer(NewParameterSource->GetLayerHandle());
				ExternalParamCache.ParameterSources.Add(MoveTemp(NewParameterSource));
			}
		}
	}

	// Duplicate intermediate data area
	IntermediatesData = InSchedule->IntermediatesData;

	// Make a hosting layer for the intermediates
	IntermediatesLayer = FParamStack::MakeReferenceLayer(NAME_None, IntermediatesData);

	// Resize remapped intermediate data layers for port tasks, they will be allocated lazily later
	PortTermLayers.SetNum(InSchedule->Ports.Num());
}

FScheduleInstanceData::~FScheduleInstanceData() = default;

void FScheduleInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (const TPair<FName, FUserScope>& ParamPair : UserScopes)
	{
		for(const TPair<FName, TUniquePtr<IParameterSource>>& SourcePair : ParamPair.Value.BeforeSources)
		{
			if(SourcePair.Value.IsValid())
			{
				SourcePair.Value->AddReferencedObjects(Collector);
			}
		}
		for(const TPair<FName, TUniquePtr<IParameterSource>>& SourcePair : ParamPair.Value.AfterSources)
		{
			if(SourcePair.Value.IsValid())
			{
				SourcePair.Value->AddReferencedObjects(Collector);
			}
		}
	}

	for (FGraphCache& GraphCache : GraphCaches)
	{
		Collector.AddPropertyReferencesWithStructARO(FAnimNextGraphInstancePtr::StaticStruct(), &GraphCache.GraphInstanceData);
	}

	for(const FExternalParamCache& ExternalParamCache : ExternalParamCaches)
	{
		for(const TUniquePtr<IParameterSource>& ParameterSource : ExternalParamCache.ParameterSources)
		{
			ParameterSource->AddReferencedObjects(Collector);
		}
	}
}

FString FScheduleInstanceData::GetReferencerName() const
{
	return TEXT("AnimNextInstanceData");
}

TSharedPtr<FParamStack> FScheduleInstanceData::GetParamStack(uint32 InIndex) const
{
	if(InIndex == MAX_uint32)
	{
		return RootParamStack;
	}
	else
	{
		return ParamStacks[InIndex];
	}
}

void FScheduleInstanceData::ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, TUniquePtr<IParameterSource>&& InParameters)
{
	if (InScope == NAME_None)
	{
		RootUserScopes.Add(MoveTemp(InParameters));
	}
	else // apply to specified scope
	{
		FScheduleInstanceData::FUserScope& ScopeSource = UserScopes.FindOrAdd(InScope);
		switch (InOrdering)
		{
		default:
		case EParameterScopeOrdering::Before:
			ScopeSource.BeforeSources.Add(InParameters->GetInstanceId(), MoveTemp(InParameters));
			break;
		case EParameterScopeOrdering::After:
			ScopeSource.AfterSources.Add(InParameters->GetInstanceId(), MoveTemp(InParameters));
			break;
		}
	}
}

void FScheduleInstanceData::QueueInputTraitEvent(FAnimNextTraitEventPtr Event)
{
	InputEventList.Push(MoveTemp(Event));
}

}