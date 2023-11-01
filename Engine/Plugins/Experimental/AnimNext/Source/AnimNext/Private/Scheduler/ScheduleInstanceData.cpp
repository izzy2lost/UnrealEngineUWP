// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedule.h"
#include "ScheduleHandle.h"
#include "Scheduler/AnimNextSchedulerWorldSubsystem.h"
#include "Scheduler/AnimNextSchedulerEntry.h"
#include "Scheduler/AnimNextSchedulePort.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_CreateInstanceData);

namespace UE::AnimNext
{

FScheduleInstanceData::FScheduleInstanceData(const FScheduleContext& InScheduleContext, const UAnimNextSchedule* InSchedule, FScheduleHandle InHandle, FAnimNextSchedulerEntry* InCurrentEntry)
	: Handle(InHandle)
	, Entry(InCurrentEntry) 
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_CreateInstanceData);

	// Preallocate data for all scopes & graphs in the schedule
	ScopeCaches.SetNum(InSchedule->ParamScopeEntryTasks.Num());
	GraphInstanceData.SetNum(InSchedule->Tasks.Num());
	GraphInputLayers.SetNum(InSchedule->Tasks.Num());

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

	// Duplicate intermediate data area
	IntermediatesData = InSchedule->IntermediatesData;

	// Make a hosting layer for the intermediates
	IntermediatesLayer = FParamStack::MakeReferenceLayer(IntermediatesData);

	// Resize remapped intermediate data layers for graph and port tasks, they will be allocated lazily later
	GraphTermLayers.SetNum(InSchedule->Tasks.Num());
	PortTermLayers.SetNum(InSchedule->Ports.Num());
}

void FScheduleInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<FName, FAnimNextParameterCollection>& ParamPair : UserScopes)
	{
		Collector.AddPropertyReferencesWithStructARO(FAnimNextParameterCollection::StaticStruct(), &ParamPair.Value);
	}

	for (FAnimNextGraphInstance& GraphInstance : GraphInstanceData)
	{
		Collector.AddPropertyReferencesWithStructARO(FAnimNextGraphInstance::StaticStruct(), &GraphInstance);
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

}