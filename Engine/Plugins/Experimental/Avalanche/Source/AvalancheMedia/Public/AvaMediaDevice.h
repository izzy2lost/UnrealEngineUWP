// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MediaIOCoreDefinitions.h"

struct FAvaMediaDevice
{
	FAvaMediaDevice(const FMediaIODevice& InDevice, const FName& InDeviceProviderName, const FString& InServerName)
		: Device(InDevice)
		, DeviceProviderName(InDeviceProviderName)
		, ServerName(InServerName)
	{
	}
	
	bool IsValid() const
	{
		return Device.DeviceName != NAME_None && Device.IsValid();
	}
	
	bool operator==(const FAvaMediaDevice& Other) const
	{
		return Device.DeviceName == Other.Device.DeviceName
			&& Device.DeviceIdentifier == Other.Device.DeviceIdentifier
			&& ServerName == Other.ServerName;
	}
	
	FText GetDisplayNameText() const
	{
		return FText::Format(NSLOCTEXT("AvalancheMedia", "AvaMediaDeviceDisplayName", "{0}")
			, FText::FromName(Device.DeviceName));
	}

	friend uint32 GetTypeHash(const FAvaMediaDevice& InDevice)
	{
		uint32 Hash = GetTypeHash(InDevice.Device.DeviceName);
		Hash = HashCombine(Hash, InDevice.Device.DeviceIdentifier);
		Hash = HashCombine(Hash, GetTypeHash(InDevice.ServerName));
		return Hash;
	}

	const FMediaIODevice& GetDevice() const { return Device; }
	const FName& GetDeviceProviderName() const { return DeviceProviderName;}
	const FString& GetServerName() const { return ServerName;}
	
protected:
	
	FMediaIODevice Device;

	/** Name of the device provider for this device. */
	FName DeviceProviderName;
	
	/** Name of the server hosting this device. */
	FString ServerName;
};
