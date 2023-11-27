// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextSchedule.h"
#include "Tasks/Task.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineLogs.h"

#if WITH_EDITOR
TUniqueFunction<void(UAnimNextSchedule*)> UAnimNextSchedule::CompileFunction;
#endif

void UAnimNextSchedule::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	CompileSchedule();
#endif
}

#if WITH_EDITOR

void UAnimNextSchedule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CompileSchedule();
}

void UAnimNextSchedule::PostEditUndo()
{
	Super::PostEditUndo();

	CompileSchedule();
}

void UAnimNextSchedule::CompileSchedule()
{
	check(CompileFunction);

	CompileFunction(this);
}

#endif // #if WITH_EDITOR