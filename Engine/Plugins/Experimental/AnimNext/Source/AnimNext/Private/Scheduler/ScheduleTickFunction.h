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

struct FScheduleTickFunction : public FTickFunction
{
	FScheduleTickFunction(const FScheduleContext& InScheduleContext, TConstArrayView<FAnimNextScheduleInstruction> InInstructions, TConstArrayView<TWeakObjectPtr<UObject>> InTargetObjects)
		: ScheduleContext(InScheduleContext)
		, Instructions(InInstructions)
		, TargetObjects(InTargetObjects)
	{
		bCanEverTick = true;
		bStartWithTickEnabled = true;
		bRunOnAnyThread = true;
	}

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;

	// Static helpers for running a slice of instructions
	// This can be called standalone to run a whole schedule in one call
	static void RunSchedule(TConstArrayView<FAnimNextScheduleInstruction> InInstructions);
	static void RunSchedule(TConstArrayView<FAnimNextScheduleInstruction> InInstructions, TConstArrayView<TWeakObjectPtr<UObject>> InTargetObjects, TFunctionRef<void(void)> InPreExecuteScope, TFunctionRef<void(void)> InPostExecuteScope);

	const FScheduleContext& ScheduleContext;
	TConstArrayView<FAnimNextScheduleInstruction> Instructions;
	TArray<FTickPrerequisite> Subsequents;
	TConstArrayView<TWeakObjectPtr<UObject>> TargetObjects;
	TSpscQueue<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> PreExecuteTasks;
	TSpscQueue<TUniqueFunction<void(const UE::AnimNext::FScheduleContext&)>> PostExecuteTasks;
};

}