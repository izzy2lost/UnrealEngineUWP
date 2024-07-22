// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreTimeSourceBase.h"

#include "Animators/PropertyAnimatorCoreBase.h"

#if WITH_EDITOR
FName UPropertyAnimatorCoreTimeSourceBase::GetTimeElapsedPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreTimeSourceBase, TimeElapsed);
}
#endif

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

	TimeElapsed = GetTimeElapsed();

	if (!IsValidTimeElapsed(TimeElapsed))
	{
		return TOptional<double>();
	}

	return TimeElapsed;
}

UPropertyAnimatorCoreBase* UPropertyAnimatorCoreTimeSourceBase::GetAnimator() const
{
	return GetTypedOuter<UPropertyAnimatorCoreBase>();
}
