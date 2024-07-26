// Copyright Epic Games, Inc. All Rights Reserved.

#include "DaySequenceHelpers.h"

#include "Misc/FrameRate.h"
#include "Misc/QualifiedFrameTime.h"

float DaySequenceHelpers::TimecodeToHours(const FDaySequenceTime& InTimecode, const TOptional<FFrameRate>& InRate)
{
	return TimecodeToSeconds(InTimecode, InRate) / FDaySequenceTime::SecondsPerHour;
}

float DaySequenceHelpers::TimecodeToSeconds(const FDaySequenceTime& InTimecode, const TOptional<FFrameRate>& InRate)
{
	float Result;
	
	Result = InTimecode.Hours * FDaySequenceTime::SecondsPerHour + InTimecode.Minutes * FDaySequenceTime::SecondsPerMinute + InTimecode.Seconds; 

	return Result;
}

FDaySequenceTime DaySequenceHelpers::HoursToTimecode(float InHours, const TOptional<FFrameRate>& InRate)
{
	return SecondsToTimecode(InHours * FDaySequenceTime::SecondsPerHour, InRate);
}

FDaySequenceTime DaySequenceHelpers::SecondsToTimecode(float InSeconds, const TOptional<FFrameRate>& InRate)
{
	FDaySequenceTime Result;

	float Seconds = InSeconds;
	const float Hours = FMath::Floor(InSeconds / FDaySequenceTime::SecondsPerHour);
	Seconds -= Hours * FDaySequenceTime::SecondsPerHour;
	const float Minutes = FMath::Floor(Seconds / FDaySequenceTime::SecondsPerMinute);
	Seconds -= Minutes * FDaySequenceTime::SecondsPerMinute;
	Result.Hours = Hours;
	Result.Minutes = Minutes;
	Result.Seconds = FMath::Floor(Seconds);
	
	return Result;
}

