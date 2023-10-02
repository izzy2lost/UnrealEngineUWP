// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextSchedulePortTask.generated.h"

class UAnimNextSchedule;

namespace UE::AnimNext
{
	struct FScheduleContext;
	struct FScheduleTask;
	struct FScheduleTickFunction;
}

USTRUCT()
struct FAnimNextSchedulePortTask
{
	GENERATED_BODY()

	friend struct UE::AnimNext::FScheduleTask;
	friend struct UE::AnimNext::FScheduleTickFunction;
	friend class UAnimNextSchedule;

	FAnimNextSchedulePortTask() = default;

private:
	void RunPort(const UE::AnimNext::FScheduleContext& InScheduleContext) const;

private:
	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	// The name of the port
	UPROPERTY()
	FName Name;

	// The name of the ports input parameter
	UPROPERTY()
	FName InputParameterName;
};
