// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedule.h"
#include "ScheduleHandle.h"
#include "Scheduler/AnimNextSchedulerWorldSubsystem.h"
#include "Scheduler/AnimNextSchedulerEntry.h"

namespace UE::AnimNext
{

FScheduleInstanceData::FScheduleInstanceData(const FScheduleContext& InScheduleContext, const UAnimNextSchedule* InSchedule, FScheduleHandle InHandle, FAnimNextSchedulerEntry* InCurrentEntry, TMap<FName, FAnimNextParameterCollection>&& InUserScopes)
	: Handle(InHandle)
	, Entry(InCurrentEntry) 
	, UserScopes(MoveTemp(InUserScopes))
{
	// Preallocate data for all scopes & graphs in the schedule
	ScopeCaches.SetNum(InSchedule->ParamScopeEntryTasks.Num());
	GraphInstanceData.SetNum(InSchedule->Tasks.Num());

	// Setup param stack graph
	RootParamStack = InCurrentEntry->RootParamStack;
	ParamStacks.SetNum(InSchedule->NumParameterScopes);
	for (TSharedPtr<FParamStack>& ParamStack : ParamStacks)
	{
		ParamStack = MakeShared<FParamStack>();
	}

	for (const FAnimNextScheduleGraphTask& Task : InSchedule->Tasks)
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
}

void FScheduleInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<FName, FAnimNextParameterCollection>& ParamPair : UserScopes)
	{
		Collector.AddPropertyReferencesWithStructARO(FAnimNextParameterCollection::StaticStruct(), &ParamPair.Value);
	}
}

FString FScheduleInstanceData::GetReferencerName() const
{
	return TEXT("AnimNextInstanceData");
}

}