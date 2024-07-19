// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsMMCvarUtils.h"
#include "HAL/IConsoleManager.h"


static int32 EnableDetailedWindowsDeviceLoggingCVar = 0;
FAutoConsoleVariableRef CVarEnableDetailedWindowsDeviceLogging(
	TEXT("au.EnableDetailedWindowsDeviceLogging"),
	EnableDetailedWindowsDeviceLoggingCVar,
	TEXT("Enables detailed windows device logging.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default);

namespace Audio
{
	bool WindowsMMCvarUtils::ShouldLogDeviceSwaps()
	{
		return EnableDetailedWindowsDeviceLoggingCVar != 0;
	}

} // namespace Audio
