// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/AnimNext_LODPose.h"
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

	FAnimNextScheduleGraphTask() = default;

private:
	friend class UAnimNextComponent;
	friend class UAnimNextSchedule;
	friend struct UE::AnimNext::FScheduleInstanceData;
	friend struct UE::AnimNext::FScheduleTickFunction;

	void RunGraph(const UE::AnimNext::FScheduleContext& InContext) const;

private:
	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamParentScopeIndex = MAX_uint32;

	UPROPERTY()
	FName EntryPoint;

	UPROPERTY()
	TObjectPtr<UAnimNextGraph> Graph = nullptr;

	UPROPERTY()
	FName DynamicGraph;

	// Index of each term in the schedule intermediates
	UPROPERTY()
	TArray<uint32> Terms;
};
