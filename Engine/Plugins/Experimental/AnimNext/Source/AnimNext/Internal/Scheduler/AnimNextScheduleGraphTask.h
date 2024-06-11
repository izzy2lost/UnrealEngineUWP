// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/AnimNext_LODPose.h"
#include "Param/AnimNextParam.h"
#include "Param/ParamId.h"
#include "AnimNextScheduleGraphTask.generated.h"

class UAnimNextModule;
struct FAnimNextEditorParam;
struct FAnimNextParamUniversalObjectLocator;
template<typename T> struct TInstancedStruct;

namespace UE::AnimNext
{
	struct FScheduleContext;
	struct FScheduleInstanceData;
	struct FScheduleTickFunction;
	struct FParamStack;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

USTRUCT()
struct FAnimNextScheduleGraphTask
{
	GENERATED_BODY()

	FAnimNextScheduleGraphTask() = default;

private:
	friend class UAnimNextComponent;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::FScheduleInstanceData;
	friend struct UE::AnimNext::FScheduleTickFunction;

	void RunModule(const UE::AnimNext::FScheduleContext& InContext) const;

	UAnimNextModule* GetModuleToRun(UE::AnimNext::FParamStack& ParamStack) const;

	// Verify graph's required parameters are satisfied by this task's supplied parameters
	void VerifyRequiredParameters(UAnimNextModule* InModuleToRun) const;

private:
	UPROPERTY()
	uint32 TaskIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamScopeIndex = MAX_uint32;

	UPROPERTY()
	uint32 ParamParentScopeIndex = MAX_uint32;

	UPROPERTY()
	FAnimNextParam EntryPoint;

	UPROPERTY()
	TObjectPtr<UAnimNextModule> Module = nullptr;

	UPROPERTY()
	FAnimNextParam DynamicModule;

	UPROPERTY()
	FAnimNextParam ReferencePose;

	UPROPERTY()
	FAnimNextParam LOD;

	// Index of each term in the schedule intermediates
	UPROPERTY()
	TArray<uint32> Terms;

	UPROPERTY()
	uint64 SuppliedParametersHash = 0;

	// All supplied parameters for any dynamic graphs slotted here
	UPROPERTY()
	TArray<FAnimNextParam> SuppliedParameters;
};
