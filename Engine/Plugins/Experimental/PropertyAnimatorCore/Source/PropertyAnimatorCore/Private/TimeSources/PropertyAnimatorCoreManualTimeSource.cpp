// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreManualTimeSource.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/App.h"

void UPropertyAnimatorCoreManualTimeSource::SetOverrideTime(bool bInOverride)
{
	if (bOverrideTime == bInOverride)
	{
		return;
	}

	bOverrideTime = bInOverride;
	OnOverrideTimeChanged();
}

void UPropertyAnimatorCoreManualTimeSource::SetCustomTime(double InTime)
{
	if (!bOverrideTime || FMath::IsNearlyEqual(InTime, CustomTime))
	{
		return;
	}

	CustomTime = InTime;
}

void UPropertyAnimatorCoreManualTimeSource::SetState(const FPropertyAnimatorCoreManualState& InState)
{
	if (State.Status == InState.Status)
	{
		return;
	}

	State = InState;
	OnStateChanged();
}

bool UPropertyAnimatorCoreManualTimeSource::UpdateEvaluationData(FPropertyAnimatorCoreTimeSourceEvaluationData& OutData)
{
	if (!bOverrideTime)
	{
		/*
		 * Don't use world delta time to avoid time dilation,
		 * get the app to use raw time between frames and increment when this time source is enabled
		 */
		if (ActiveStatus == EPropertyAnimatorCoreManualStatus::PlayingForward)
		{
			CustomTime += FApp::GetDeltaTime();
		}
		else if (ActiveStatus == EPropertyAnimatorCoreManualStatus::PlayingBackward)
		{
			CustomTime -= FApp::GetDeltaTime();
		}

		OutData.TimeElapsed = CustomTime;

		return ActiveStatus != EPropertyAnimatorCoreManualStatus::Stopped;
	}

	OutData.TimeElapsed = CustomTime;

	return true;
}

void UPropertyAnimatorCoreManualTimeSource::OnTimeSourceActive()
{
	Super::OnTimeSourceActive();

	ActiveStatus = EPropertyAnimatorCoreManualStatus::Stopped;
}

void UPropertyAnimatorCoreManualTimeSource::OnTimeSourceInactive()
{
	Super::OnTimeSourceInactive();

	Stop();
}

bool UPropertyAnimatorCoreManualTimeSource::ImportPreset(const UPropertyAnimatorCorePresetBase* InPreset, const TSharedRef<FJsonValue>& InValue)
{
	const TSharedPtr<FJsonObject>* JsonTimeSourceObject;

	if (Super::ImportPreset(InPreset, InValue) && InValue->TryGetObject(JsonTimeSourceObject))
	{
		double JsonCustomTime = CustomTime;
		(*JsonTimeSourceObject)->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreManualTimeSource, CustomTime), JsonCustomTime);
		SetCustomTime(JsonCustomTime);

		bool bJsonOverrideTime = bOverrideTime;
		(*JsonTimeSourceObject)->TryGetBoolField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreManualTimeSource, bOverrideTime), bJsonOverrideTime);
		SetOverrideTime(bJsonOverrideTime);

		return true;
	}

	return false;
}

bool UPropertyAnimatorCoreManualTimeSource::ExportPreset(const UPropertyAnimatorCorePresetBase* InPreset, TSharedPtr<FJsonValue>& OutValue)
{
	const TSharedPtr<FJsonObject>* JsonTimeSourceObject;

	if (Super::ExportPreset(InPreset, OutValue) && OutValue->TryGetObject(JsonTimeSourceObject))
	{
		(*JsonTimeSourceObject)->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreManualTimeSource, CustomTime), CustomTime);
		(*JsonTimeSourceObject)->SetBoolField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorCoreManualTimeSource, bOverrideTime), bOverrideTime);

		return true;
	}

	return false;
}

void UPropertyAnimatorCoreManualTimeSource::Play(bool bInForward)
{
	if (bOverrideTime)
	{
		return;
	}

	// Allow change from playing forward to backward
	if (!IsPlaying()
		|| (bInForward && ActiveStatus == EPropertyAnimatorCoreManualStatus::PlayingBackward)
		|| (!bInForward && ActiveStatus == EPropertyAnimatorCoreManualStatus::PlayingForward))
	{
		ActiveStatus = bInForward ? EPropertyAnimatorCoreManualStatus::PlayingForward : EPropertyAnimatorCoreManualStatus::PlayingBackward;
	}
}

void UPropertyAnimatorCoreManualTimeSource::Pause()
{
	if (!IsPlaying())
	{
		return;
	}

	ActiveStatus = EPropertyAnimatorCoreManualStatus::Paused;
}

void UPropertyAnimatorCoreManualTimeSource::Stop()
{
	if (ActiveStatus == EPropertyAnimatorCoreManualStatus::Stopped)
	{
		return;
	}

	Pause();
	ActiveStatus = EPropertyAnimatorCoreManualStatus::Stopped;
}

EPropertyAnimatorCoreManualStatus UPropertyAnimatorCoreManualTimeSource::GetPlaybackStatus() const
{
	return bOverrideTime ? EPropertyAnimatorCoreManualStatus::Stopped : ActiveStatus;
}

bool UPropertyAnimatorCoreManualTimeSource::IsPlaying() const
{
	return !bOverrideTime && (ActiveStatus == EPropertyAnimatorCoreManualStatus::PlayingBackward || ActiveStatus == EPropertyAnimatorCoreManualStatus::PlayingForward);
}

#if WITH_EDITOR
void UPropertyAnimatorCoreManualTimeSource::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreManualTimeSource, State))
	{
		OnStateChanged();
	}
	else if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreManualTimeSource, bOverrideTime))
	{
		OnOverrideTimeChanged();
	}
}
#endif

void UPropertyAnimatorCoreManualTimeSource::OnStateChanged()
{
	switch (State.Status)
	{
	case EPropertyAnimatorCoreManualStatus::Stopped:
		Stop();
		break;
	case EPropertyAnimatorCoreManualStatus::Paused:
		Pause();
		break;
	case EPropertyAnimatorCoreManualStatus::PlayingForward:
		Play(/** Forward */true);
		break;
	case EPropertyAnimatorCoreManualStatus::PlayingBackward:
		Play(/** Forward */false);
		break;
	}
}

void UPropertyAnimatorCoreManualTimeSource::OnOverrideTimeChanged()
{
	Stop();
	CustomTime = 0.f;
	State.Status = EPropertyAnimatorCoreManualStatus::Stopped;
}
