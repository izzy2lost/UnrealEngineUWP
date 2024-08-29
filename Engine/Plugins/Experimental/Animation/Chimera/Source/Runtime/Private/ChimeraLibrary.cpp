// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chimera/ChimeraLibrary.h"
#include "Chimera/ChimeraSubsystem.h"
#include "Engine/World.h"

FChimeraBlueprintResult UChimeraLibrary::ChimeraQuery_Pure(TArray<FChimeraAvailability> Availabilities, UObject* AnimInstance, FName PoseHistoryName, bool bValidateResultAgainstAvailabilities)
{
	FChimeraBlueprintResult Result;
	if (UChimeraSubsystem* ChimeraSubsystem = UChimeraSubsystem::GetSubsystem_AnyThread(AnimInstance))
	{
		ChimeraSubsystem->Query_AnyThread(Availabilities, AnimInstance, Result, PoseHistoryName, nullptr, bValidateResultAgainstAvailabilities);
	}
	return Result;
}

FChimeraBlueprintResult UChimeraLibrary::ChimeraQuery(TArray<FChimeraAvailability> Availabilities, UObject* AnimInstance, FName PoseHistoryName, bool bValidateResultAgainstAvailabilities)
{
	return ChimeraQuery_Pure(Availabilities, AnimInstance, PoseHistoryName, bValidateResultAgainstAvailabilities);
}

FChimeraBlueprintResult UChimeraLibrary::ChimeraQuery(const TArrayView<const FChimeraAvailability> Availabilities, UObject* AnimInstance, const FAnimNode_PoseSearchHistoryCollector_Base* HistoryCollector, bool bValidateResultAgainstAvailabilities)
{
	FChimeraBlueprintResult Result;
	if (UChimeraSubsystem* ChimeraSubsystem = UChimeraSubsystem::GetSubsystem_AnyThread(AnimInstance))
	{
		ChimeraSubsystem->Query_AnyThread(Availabilities, AnimInstance, Result, FName(), HistoryCollector, bValidateResultAgainstAvailabilities);
	}
	return Result;
}
