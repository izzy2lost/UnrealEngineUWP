// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextScheduleExternalTask.generated.h"

class UAnimNextSchedule;
class UAnimNextComponent;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FScheduleInstanceData;
	struct FScheduleTickFunction;
}

USTRUCT()
struct FAnimNextScheduleExternalTask
{
	GENERATED_BODY()

	FAnimNextScheduleExternalTask() = default;

private:
	friend class UAnimNextSchedule;
	friend class UAnimNextComponent;
	friend struct FAnimNextSchedulerEntry;
	friend struct UE::AnimNext::FScheduleTickFunction;
	friend struct UE::AnimNext::FScheduleInstanceData;

	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamParentScopeIndex = MAX_uint32;

	/** The name of the external task parameter */
	UPROPERTY()
	FName Name;

	/** The name of the external task's object parameter */
	UPROPERTY()
	FName ObjectName;
};
