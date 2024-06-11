// Copyright Epic Games, Inc. All Rights Reserved.

#include "DaySequenceHelpers.h"
#include "Misc/FrameRate.h"
#include "Misc/QualifiedFrameTime.h"

static constexpr float SecondsPerHour = 3600.f;
static constexpr float SecondsPerMinute = 60.f;

float DaySequenceHelpers::TimecodeToHours(const FTimecode& InTimecode, const TOptional<FFrameRate>& InRate)
{
	return TimecodeToSeconds(InTimecode, InRate) / SecondsPerHour;
}

float DaySequenceHelpers::TimecodeToSeconds(const FTimecode& InTimecode, const TOptional<FFrameRate>& InRate)
{
	float Result;
	if (InRate.IsSet())
	{
		const FQualifiedFrameTime FrameTime(InTimecode, *InRate);
		Result = FrameTime.AsSeconds();
	}
	else
	{
		Result = InTimecode.Hours * SecondsPerHour + InTimecode.Minutes * SecondsPerMinute + InTimecode.Seconds; 
	}
	return Result;
}

FTimecode DaySequenceHelpers::HoursToTimecode(float InHours, const TOptional<FFrameRate>& InRate)
{
	return SecondsToTimecode(InHours * SecondsPerHour, InRate);
}

FTimecode DaySequenceHelpers::SecondsToTimecode(float InSeconds, const TOptional<FFrameRate>& InRate)
{
	FTimecode Result;
	if (InRate.IsSet())
	{
		Result = FQualifiedFrameTime(InRate->AsFrameTime(InSeconds), *InRate).ToTimecode();
	}
	else
	{
		float Seconds = InSeconds;
		const float Hours = FMath::Floor(InSeconds / SecondsPerHour);
		Seconds -= Hours * SecondsPerHour;
		const float Minutes = FMath::Floor(Seconds / SecondsPerMinute);
		Seconds -= Minutes * SecondsPerMinute;
		Result.Hours = Hours;
		Result.Minutes = Minutes;
		Result.Seconds = FMath::Floor(Seconds);
	}
	return Result;
}

