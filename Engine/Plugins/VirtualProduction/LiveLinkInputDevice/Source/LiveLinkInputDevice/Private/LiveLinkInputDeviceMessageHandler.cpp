// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkInputDeviceMessageHandler.h"
#include "LiveLinkInputDeviceTypes.h"
#include "Misc/CoreMiscDefines.h"


bool FLiveLinkInputDeviceMessageHandler::OnControllerAnalog(FGamepadKeyNames::Type KeyName, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId, float AnalogValue)
{
	FLiveLinkGamepadInputDeviceFrameData& InputDeviceFrameData = CurrentFrameDataValues.FindOrAdd(InputDeviceId);
	InputDeviceFrameData.ApplyValueFromKey(KeyName, AnalogValue);
	return true;
}

bool FLiveLinkInputDeviceMessageHandler::OnControllerButtonPressed(FGamepadKeyNames::Type KeyName, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId, bool IsRepeat)
{
	FLiveLinkGamepadInputDeviceFrameData& InputDeviceFrameData = CurrentFrameDataValues.FindOrAdd(InputDeviceId);
	InputDeviceFrameData.ApplyValueFromKey(KeyName, 1);
	return true;
}

bool FLiveLinkInputDeviceMessageHandler::OnControllerButtonReleased(FGamepadKeyNames::Type KeyName, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId, bool IsRepeat)
{
	FLiveLinkGamepadInputDeviceFrameData& InputDeviceFrameData = CurrentFrameDataValues.FindOrAdd(InputDeviceId);
	InputDeviceFrameData.ApplyValueFromKey(KeyName, 0);
	return true;
}

FLiveLinkGamepadInputDeviceFrameData FLiveLinkInputDeviceMessageHandler::GetLatestValue(FInputDeviceId InDeviceId /*unused*/) const
{
	for (const TPair<FInputDeviceId, FLiveLinkGamepadInputDeviceFrameData>& Device : CurrentFrameDataValues)
	{
		return Device.Value;
	}
	return {};
}

TSet<FInputDeviceId> FLiveLinkInputDeviceMessageHandler::GetDeviceIds() const
{
	TSet<FInputDeviceId> Devices;
	CurrentFrameDataValues.GetKeys(Devices);
	return Devices;
}
