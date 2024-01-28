// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreManualTimeSource.h"

void UPropertyAnimatorCoreManualTimeSource::SetCustomTime(float InTime)
{
	if (FMath::IsNearlyEqual(InTime, CustomTime))
	{
		return;
	}

	CustomTime = InTime;
}

double UPropertyAnimatorCoreManualTimeSource::GetTimeElapsed()
{
	return CustomTime;
}

bool UPropertyAnimatorCoreManualTimeSource::IsTimeSourceReady() const
{
	return true;
}