// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreTimeSourceBase.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void UPropertyAnimatorCoreTimeSourceBase::ActivateTimeSource()
{
	if (IsTimeSourceActive())
	{
		return;
	}

	bTimeSourceActive = true;
	OnTimeSourceActive();
}

void UPropertyAnimatorCoreTimeSourceBase::DeactivateTimeSource()
{
	if (!IsTimeSourceActive())
	{
		return;
	}

	bTimeSourceActive = false;
	OnTimeSourceInactive();
}

EPropertyAnimatorCoreTimeSourceResult UPropertyAnimatorCoreTimeSourceBase::FetchEvaluationData(FPropertyAnimatorCoreTimeSourceEvaluationData& OutEvaluationData)
{
	if (!UpdateEvaluationData(OutEvaluationData))
	{
		// Reset evaluation state
		return EPropertyAnimatorCoreTimeSourceResult::Idle;
	}

	if (!IsFramerateAllowed(OutEvaluationData.TimeElapsed))
	{
		// Skip evaluation for this run
		return EPropertyAnimatorCoreTimeSourceResult::Skip;
	}

	LastTimeElapsed = OutEvaluationData.TimeElapsed;

	return EPropertyAnimatorCoreTimeSourceResult::Evaluate;
}

void UPropertyAnimatorCoreTimeSourceBase::SetFrameRate(float InFrameRate)
{
	FrameRate = FMath::Max(UE_KINDA_SMALL_NUMBER, InFrameRate);
}

void UPropertyAnimatorCoreTimeSourceBase::SetUseFrameRate(bool bInUseFrameRate)
{
	bUseFrameRate = bInUseFrameRate;
}

bool UPropertyAnimatorCoreTimeSourceBase::ImportPreset(const UPropertyAnimatorCorePresetBase* InPreset, const TSharedRef<FJsonValue>& InValue)
{
	const TSharedPtr<FJsonObject>* JsonTimeSourceObject;
	if (!InValue->TryGetObject(JsonTimeSourceObject) || !JsonTimeSourceObject)
	{
		return false;
	}

	bool bJsonUseFrameRate = bUseFrameRate;
	(*JsonTimeSourceObject)->TryGetBoolField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreTimeSourceBase, bUseFrameRate), bJsonUseFrameRate);
	SetUseFrameRate(bJsonUseFrameRate);

	double JsonFrameRate = FrameRate;
	(*JsonTimeSourceObject)->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreTimeSourceBase, FrameRate), JsonFrameRate);
	SetFrameRate(JsonFrameRate);

	return true;
}

bool UPropertyAnimatorCoreTimeSourceBase::ExportPreset(const UPropertyAnimatorCorePresetBase* InPreset, TSharedPtr<FJsonValue>& OutValue)
{
	TSharedRef<FJsonObject> JsonTimeSourceObject = MakeShared<FJsonObject>();
	OutValue = MakeShared<FJsonValueObject>(JsonTimeSourceObject);

	JsonTimeSourceObject->SetBoolField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreTimeSourceBase, bUseFrameRate), bUseFrameRate);
	JsonTimeSourceObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreTimeSourceBase, FrameRate), FrameRate);

	return true;
}

bool UPropertyAnimatorCoreTimeSourceBase::UpdateEvaluationData(FPropertyAnimatorCoreTimeSourceEvaluationData& OutData)
{
	return false;
}

bool UPropertyAnimatorCoreTimeSourceBase::IsFramerateAllowed(double InNewTime) const
{
	return !bUseFrameRate || FMath::IsNearlyZero(FrameRate) || FMath::Abs(InNewTime - LastTimeElapsed) > FMath::Abs(1.f / FrameRate);
}
