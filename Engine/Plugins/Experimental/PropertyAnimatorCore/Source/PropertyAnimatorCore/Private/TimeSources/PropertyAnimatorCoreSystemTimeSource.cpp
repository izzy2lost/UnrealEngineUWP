// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreSystemTimeSource.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Internationalization/Regex.h"
#include "Misc/DateTime.h"

bool UPropertyAnimatorCoreSystemTimeSource::UpdateEvaluationData(FPropertyAnimatorCoreTimeSourceEvaluationData& OutData)
{
	switch(Mode)
	{
	case EPropertyAnimatorCoreSystemMode::LocalTime:
		{
			OutData.TimeElapsed = (FDateTime::Now() - FDateTime::MinValue()).GetTotalSeconds();
		}
		break;
	case EPropertyAnimatorCoreSystemMode::UtcTime:
		{
			OutData.TimeElapsed = (FDateTime::UtcNow() - FDateTime::MinValue()).GetTotalSeconds();
		}
		break;
	case EPropertyAnimatorCoreSystemMode::Countdown:
		{
			OutData.TimeElapsed = (CountdownTimeSpan - (FDateTime::Now() - ActivationTime)).GetTotalSeconds();
		}
		break;
	case EPropertyAnimatorCoreSystemMode::Stopwatch:
		{
			OutData.TimeElapsed = (FDateTime::Now() - ActivationTime).GetTotalSeconds();
		}
		break;
	}

	return true;
}

void UPropertyAnimatorCoreSystemTimeSource::OnTimeSourceActive()
{
	Super::OnTimeSourceActive();

	ActivationTime = FDateTime::Now();
	OnModeChanged();
}

bool UPropertyAnimatorCoreSystemTimeSource::ImportPreset(const UPropertyAnimatorCorePresetBase* InPreset, const TSharedRef<FJsonValue>& InValue)
{
	const TSharedPtr<FJsonObject>* JsonTimeSourceObject;

	if (Super::ImportPreset(InPreset, InValue) && InValue->TryGetObject(JsonTimeSourceObject))
	{
		FString JsonCountdown = CountdownDuration;
		(*JsonTimeSourceObject)->TryGetStringField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreSystemTimeSource, CountdownDuration), JsonCountdown);
		SetCountdownDuration(JsonCountdown);

		double JsonMode = static_cast<double>(Mode);
		(*JsonTimeSourceObject)->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreSystemTimeSource, Mode), JsonMode);
		SetMode(static_cast<EPropertyAnimatorCoreSystemMode>(JsonMode));

		return true;
	}

	return false;
}

bool UPropertyAnimatorCoreSystemTimeSource::ExportPreset(const UPropertyAnimatorCorePresetBase* InPreset, TSharedPtr<FJsonValue>& OutValue)
{
	TSharedPtr<FJsonObject>* JsonTimeSourceObject = nullptr;

	if (Super::ExportPreset(InPreset, OutValue) && OutValue->TryGetObject(JsonTimeSourceObject))
	{
		(*JsonTimeSourceObject)->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreSystemTimeSource, Mode), static_cast<double>(Mode));
		(*JsonTimeSourceObject)->SetStringField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreSystemTimeSource, CountdownDuration), CountdownDuration);

		return true;
	}

	return false;
}

void UPropertyAnimatorCoreSystemTimeSource::SetMode(EPropertyAnimatorCoreSystemMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	OnModeChanged();
}

void UPropertyAnimatorCoreSystemTimeSource::SetCountdownDuration(const FTimespan& InTimeSpan)
{
	if (InTimeSpan == CountdownTimeSpan)
	{
		return;
	}

	SetCountdownDuration(InTimeSpan.ToString(TEXT("%h:%m:%s")));
}

void UPropertyAnimatorCoreSystemTimeSource::SetCountdownDuration(const FString& InDuration)
{
	if (CountdownDuration == InDuration)
	{
		return;
	}

	CountdownDuration = InDuration;
	OnModeChanged();
}

#if WITH_EDITOR
void UPropertyAnimatorCoreSystemTimeSource::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreSystemTimeSource, Mode)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreSystemTimeSource, CountdownDuration))
	{
		OnModeChanged();
	}
}
#endif

void UPropertyAnimatorCoreSystemTimeSource::OnModeChanged()
{
	if (Mode == EPropertyAnimatorCoreSystemMode::Countdown)
	{
		CountdownTimeSpan = ParseTime(CountdownDuration);
	}
}

FTimespan UPropertyAnimatorCoreSystemTimeSource::ParseTime(const FString& InFormat)
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
