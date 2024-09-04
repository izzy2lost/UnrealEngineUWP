// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chimera/ChimeraLibrary.h"
#include "Animation/AnimMontage.h"
#include "Chimera/ChimeraSubsystem.h"
#include "Engine/World.h"

FChimeraBlueprintResult UChimeraLibrary::ChimeraQuery_Pure(TArray<FChimeraAvailability> Availabilities, UObject* AnimInstance, FPoseSearchContinuingProperties ContinuingProperties, FName PoseHistoryName, bool bValidateResultAgainstAvailabilities)
{
	FChimeraBlueprintResult Result;
	if (UChimeraSubsystem* ChimeraSubsystem = UChimeraSubsystem::GetSubsystem_AnyThread(AnimInstance))
	{
		ChimeraSubsystem->Query_AnyThread(Availabilities, AnimInstance, ContinuingProperties, Result, PoseHistoryName, nullptr, bValidateResultAgainstAvailabilities);
	}
	return Result;
}

FChimeraBlueprintResult UChimeraLibrary::ChimeraQuery(TArray<FChimeraAvailability> Availabilities, UObject* AnimInstance, FPoseSearchContinuingProperties ContinuingProperties, FName PoseHistoryName, bool bValidateResultAgainstAvailabilities)
{
	return ChimeraQuery_Pure(Availabilities, AnimInstance, ContinuingProperties, PoseHistoryName, bValidateResultAgainstAvailabilities);
}

FChimeraBlueprintResult UChimeraLibrary::ChimeraQuery(const TArrayView<const FChimeraAvailability> Availabilities, UObject* AnimInstance, const FPoseSearchContinuingProperties& ContinuingProperties, const FAnimNode_PoseSearchHistoryCollector_Base* HistoryCollector, bool bValidateResultAgainstAvailabilities)
{
	FChimeraBlueprintResult Result;
	if (UChimeraSubsystem* ChimeraSubsystem = UChimeraSubsystem::GetSubsystem_AnyThread(AnimInstance))
	{
		ChimeraSubsystem->Query_AnyThread(Availabilities, AnimInstance, ContinuingProperties, Result, FName(), HistoryCollector, bValidateResultAgainstAvailabilities);
	}
	return Result;
}

FPoseSearchContinuingProperties UChimeraLibrary::GetMontageContinuingProperties(UAnimInstance* AnimInstance)
{
	FPoseSearchContinuingProperties ContinuingProperties;
	if (const FAnimMontageInstance* AnimMontageInstance = AnimInstance->GetActiveMontageInstance())
	{
		ContinuingProperties.PlayingAsset = AnimMontageInstance->Montage;
		ContinuingProperties.PlayingAssetAccumulatedTime = AnimMontageInstance->DeltaTimeRecord.GetPrevious();
	}
	return ContinuingProperties;
}