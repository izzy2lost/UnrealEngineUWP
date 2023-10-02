// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Containers/SpscQueue.h"

struct FAnimNextScheduleInstruction;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FScheduleContext;
	struct FParamStackLayerHandle;
}

namespace UE::AnimNext
{

struct FScheduleBeginTickFunction : public FTickFunction
{
	FScheduleBeginTickFunction(FAnimNextSchedulerEntry& InEntry)
		: Entry(InEntry)
	{
		bCanEverTick = true;
		bStartWithTickEnabled = true;
		bRunOnAnyThread = true;
	}

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;

	FAnimNextSchedulerEntry& Entry;
	FTickPrerequisite Subsequent;
};

struct FScheduleEndTickFunction : public FTickFunction
{
	FScheduleEndTickFunction(FAnimNextSchedulerEntry& InEntry)
		: Entry(InEntry)
	{
		bCanEverTick = true;
		bStartWithTickEnabled = true;
		bRunOnAnyThread = true;
	}

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;

	FAnimNextSchedulerEntry& Entry;
};

// TODO: Run a linear slice of instructions rather than have each instruction a separate tick function
struct FScheduleTickFunction : public FTickFunction
{
	FScheduleTickFunction(const FScheduleContext& InScheduleContext, const FAnimNextScheduleInstruction& InInstruction)
		: ScheduleContext(InScheduleContext)
		, Instruction(InInstruction)
	{
		bCanEverTick = true;
		bStartWithTickEnabled = true;
		bRunOnAnyThread = true;
	}

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;

	const FScheduleContext& ScheduleContext;
	const FAnimNextScheduleInstruction& Instruction;
	TArray<FTickPrerequisite> Subsequents;
	TWeakObjectPtr<UObject> TargetObject;
	TSpscQueue<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> PreExecuteTasks;
	TSpscQueue<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> PostExecuteTasks;
};

}