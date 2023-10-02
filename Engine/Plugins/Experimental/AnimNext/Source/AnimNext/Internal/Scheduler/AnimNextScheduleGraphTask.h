// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextScheduleGraphTask.generated.h"

class UAnimNextGraph;
class UAnimNextParameterBlock;

namespace UE::AnimNext
{
	struct FScheduleContext;
	struct FScheduleInstanceData;
	struct FScheduleTickFunction;
}

USTRUCT()
struct FAnimNextScheduleGraphTask
{
	GENERATED_BODY()

	FAnimNextScheduleGraphTask()
	{}

private:
	friend class UAnimNextComponent;
	friend class UAnimNextSchedule;
	friend struct UE::AnimNext::FScheduleInstanceData;
	friend struct UE::AnimNext::FScheduleTickFunction;

	void RunGraph(const UE::AnimNext::FScheduleContext& InScheduleContext) const;

private:
	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamParentScopeIndex = MAX_uint32;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FName EntryPoint;

	UPROPERTY()
	TObjectPtr<UAnimNextGraph> Graph;

	UPROPERTY()
	TArray<TObjectPtr<UAnimNextParameterBlock>> ParameterBlocks;
};
