// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreTimeSourceBase.h"

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
		return EPropertyAnimatorCoreTimeSourceResult::Reset;
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

bool UPropertyAnimatorCoreTimeSourceBase::UpdateEvaluationData(FPropertyAnimatorCoreTimeSourceEvaluationData& OutData)
{
	return false;
}

bool UPropertyAnimatorCoreTimeSourceBase::IsFramerateAllowed(double InNewTime) const
{
	return !bUseFrameRate || FMath::IsNearlyZero(FrameRate) || FMath::Abs(InNewTime - LastTimeElapsed) > FMath::Abs(1.f / FrameRate);
}
