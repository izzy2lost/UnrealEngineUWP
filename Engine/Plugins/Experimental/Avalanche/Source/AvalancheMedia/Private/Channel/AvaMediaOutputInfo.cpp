// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channel/AvaMediaOutputInfo.h"
#include "OutputDevices/AvaDeviceProviderProxy.h"

bool FAvaMediaOutputInfo::IsRemote(const FString& InServerName)
{
	return !InServerName.IsEmpty() && InServerName != FAvaDeviceProviderProxyManager::LocalServerName;
}

void FAvaMediaOutputInfo::PostLoad()
{
	if (!Guid.IsValid())
	{
		Guid = FGuid::NewGuid();
	}
}
