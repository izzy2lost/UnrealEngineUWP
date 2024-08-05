// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreTimeSourceBase.h"

#include "Animators/PropertyAnimatorCoreBase.h"

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

TOptional<double> UPropertyAnimatorCoreTimeSourceBase::GetConditionalTimeElapsed()
{
	if (!IsTimeSourceReady())
	{
		return TOptional<double>();
	}

	double NewTimeElapsed = GetTimeElapsed();

	if (!IsValidTimeElapsed(NewTimeElapsed))
	{
		return TOptional<double>();
	}

	LastTimeElapsed = NewTimeElapsed;

	return LastTimeElapsed;
}

UPropertyAnimatorCoreBase* UPropertyAnimatorCoreTimeSourceBase::GetAnimator() const
{
	return GetTypedOuter<UPropertyAnimatorCoreBase>();
}

void UPropertyAnimatorCoreTimeSourceBase::SetFrameRate(float InFrameRate)
{
	FrameRate = FMath::Max(UE_KINDA_SMALL_NUMBER, InFrameRate);
}

void UPropertyAnimatorCoreTimeSourceBase::SetUseFrameRate(bool bInUseFrameRate)
{
	bUseFrameRate = bInUseFrameRate;
}

bool UPropertyAnimatorCoreTimeSourceBase::IsValidTimeElapsed(double InTimeElapsed) const
{
	return !bUseFrameRate || FMath::IsNearlyZero(FrameRate) || FMath::Abs(InTimeElapsed - LastTimeElapsed) > FMath::Abs(1.f / FrameRate);
}
