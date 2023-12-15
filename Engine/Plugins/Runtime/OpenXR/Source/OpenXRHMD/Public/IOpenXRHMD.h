// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <openxr/openxr.h>

class IOpenXRHMD
{
public:
	OPENXRHMD_API virtual bool IsInitialized() const = 0;
	OPENXRHMD_API virtual bool IsRunning() const = 0;
	OPENXRHMD_API virtual bool IsFocused() const = 0;

	OPENXRHMD_API virtual int32 AddTrackedDevice(XrAction Action, XrPath Path) = 0;
	OPENXRHMD_API virtual void ResetTrackedDevices() = 0;
	OPENXRHMD_API virtual XrPath GetTrackedDevicePath(const int32 DeviceId) = 0;
	OPENXRHMD_API virtual XrSpace GetTrackedDeviceSpace(const int32 DeviceId) = 0;

	OPENXRHMD_API virtual bool IsExtensionEnabled(const FString& Name) const = 0;
	OPENXRHMD_API virtual XrInstance GetInstance() = 0;
	OPENXRHMD_API virtual XrSystemId GetSystem() = 0;
	OPENXRHMD_API virtual XrSession GetSession() = 0;
	OPENXRHMD_API virtual XrTime GetDisplayTime() const = 0;
	OPENXRHMD_API virtual XrSpace GetTrackingSpace() const = 0;

	OPENXRHMD_API virtual class IOpenXRExtensionPluginDelegates& GetIOpenXRExtensionPluginDelegates() = 0;

	OPENXRHMD_API virtual bool GetPoseForTime(int32 DeviceId, FTimespan Timespan, bool& OutTimeWasUsed, FQuat& CurrentOrientation, FVector& CurrentPosition, bool& bProvidedLinearVelocity, FVector& LinearVelocity, bool& bProvidedAngularVelocity, FVector& AngularVelocityAsAxisAndLength, bool& bProvidedLinearAcceleration, FVector& LinearAcceleration, float WorldToMetersScale) = 0;
};

