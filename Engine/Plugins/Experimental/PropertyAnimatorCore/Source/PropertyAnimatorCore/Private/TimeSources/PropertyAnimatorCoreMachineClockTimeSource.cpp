// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreMachineClockTimeSource.h"

#include "Internationalization/Regex.h"
#include "Misc/DateTime.h"

double UPropertyAnimatorCoreMachineClockTimeSource::GetTimeElapsed()
{
	double TimeElapsedSeconds = 0;

	switch(Mode)
	{
	case EPropertyAnimatorCoreMachineClockMode::LocalTime:
		{
			TimeElapsedSeconds = (FDateTime::Now() - FDateTime::MinValue()).GetTotalSeconds();
		}
		break;
	case EPropertyAnimatorCoreMachineClockMode::UtcTime:
		{
			TimeElapsedSeconds = (FDateTime::UtcNow() - FDateTime::MinValue()).GetTotalSeconds();
		}
		break;
	case EPropertyAnimatorCoreMachineClockMode::Countdown:
		{
			TimeElapsedSeconds = (CountdownTimeSpan - (FDateTime::Now() - ActivationTime)).GetTotalSeconds();
		}
		break;
	case EPropertyAnimatorCoreMachineClockMode::Stopwatch:
		{
			TimeElapsedSeconds = (FDateTime::Now() - ActivationTime).GetTotalSeconds();
		}
		break;
	}

	return TimeElapsedSeconds;
}

bool UPropertyAnimatorCoreMachineClockTimeSource::IsTimeSourceReady() const
{
	return true;
}

void UPropertyAnimatorCoreMachineClockTimeSource::OnTimeSourceActive()
{
	Super::OnTimeSourceActive();

	ActivationTime = FDateTime::Now();
	OnModeChanged();
}

void UPropertyAnimatorCoreMachineClockTimeSource::SetMode(EPropertyAnimatorCoreMachineClockMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	OnModeChanged();
}

void UPropertyAnimatorCoreMachineClockTimeSource::SetCountdownDuration(const FTimespan& InTimeSpan)
{
	if (InTimeSpan == CountdownTimeSpan)
	{
		return;
	}

	SetCountdownDuration(InTimeSpan.ToString(TEXT("%h:%m:%s")));
}

void UPropertyAnimatorCoreMachineClockTimeSource::SetCountdownDuration(const FString& InDuration)
{
	if (CountdownDuration == InDuration)
	{
		return;
	}

	CountdownDuration = InDuration;
	OnModeChanged();
}

#if WITH_EDITOR
void UPropertyAnimatorCoreMachineClockTimeSource::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreMachineClockTimeSource, Mode)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreMachineClockTimeSource, CountdownDuration))
	{
		OnModeChanged();
	}
}
#endif

void UPropertyAnimatorCoreMachineClockTimeSource::OnModeChanged()
{
	if (Mode == EPropertyAnimatorCoreMachineClockMode::Countdown)
	{
		CountdownTimeSpan = ParseTime(CountdownDuration);
	}
}

FTimespan UPropertyAnimatorCoreMachineClockTimeSource::ParseTime(const FString& InFormat)
{
	// Regex patterns for different formats
	static const FRegexPattern HHMMSSPattern(TEXT("^(?:(\\d{2}):)?(\\d{2}):(\\d{2})$")); // 01:00 00:01:00
	static const FRegexPattern CombinedPattern(TEXT("(?:(\\d+)h)? ?(?:(\\d+)m)? ?(?:(\\d+)s)?")); // 1h 1m 1s

	FRegexMatcher HHMMSSMatcher(HHMMSSPattern, InFormat);
	FRegexMatcher CombinedMatcher(CombinedPattern, InFormat);

	FTimespan ParsedTimeSpan = FTimespan::Zero();

	if (InFormat.IsNumeric())
	{
		const int32 Seconds = FCString::Atoi(*InFormat);
		ParsedTimeSpan = FTimespan::FromSeconds(Seconds);
	}
	else if (HHMMSSMatcher.FindNext())
	{
		const int32 Hours = HHMMSSMatcher.GetCaptureGroup(1).IsEmpty() ? 0 : FCString::Atoi(*HHMMSSMatcher.GetCaptureGroup(1));
		const int32 Minutes = FCString::Atoi(*HHMMSSMatcher.GetCaptureGroup(2));
		const int32 Seconds = FCString::Atoi(*HHMMSSMatcher.GetCaptureGroup(3));
		ParsedTimeSpan = FTimespan::FromHours(Hours) + FTimespan::FromMinutes(Minutes) + FTimespan::FromSeconds(Seconds);
	}
	else if (CombinedMatcher.FindNext())
	{
		const int32 Hours = CombinedMatcher.GetCaptureGroup(1).IsEmpty() ? 0 : FCString::Atoi(*CombinedMatcher.GetCaptureGroup(1));
		const int32 Minutes = CombinedMatcher.GetCaptureGroup(2).IsEmpty() ? 0 : FCString::Atoi(*CombinedMatcher.GetCaptureGroup(2));
		const int32 Seconds = CombinedMatcher.GetCaptureGroup(3).IsEmpty() ? 0 : FCString::Atoi(*CombinedMatcher.GetCaptureGroup(3));
		ParsedTimeSpan = FTimespan::FromHours(Hours) + FTimespan::FromMinutes(Minutes) + FTimespan::FromSeconds(Seconds);
	}

	return ParsedTimeSpan;
}
