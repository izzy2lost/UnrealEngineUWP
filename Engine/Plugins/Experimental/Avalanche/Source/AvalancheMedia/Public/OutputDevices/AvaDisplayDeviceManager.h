// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/FrameRate.h"

/**
 * Contains information on physical display adapter,
 * a.k.a. graphics card.
 **/
struct FAvaDisplayAdapterInfo
{
	FString Description;			// ex: "Nvidia GeForce RTX ..."
	uint32 VendorId = 0;
	uint32 DeviceId = 0;
	uint32 SubSysId = 0;
	uint32 Revision = 0;
	uint64 DedicatedVideoMemory = 0;
};

/**
 * Contains info on a physical monitor connected to the display device.
 * Similar to FMonitorInfo, except we have the adapter description
 * and display frequency.
 */
struct FAvaMonitorInfo
{
	FAvaDisplayAdapterInfo AdapterInfo;
	FString Name;					// ex: DISPLAY1, DISPLAY2, etc (not manufacturer name)
	int32 Width = 0;				// Horizontal resolution in pixels.
	int32 Height = 0;				// Vertical resolution in pixels.
	FFrameRate DisplayFrequency = FFrameRate(0, 0);	// Default invalid.
	FPlatformRect DisplayRect;
	FPlatformRect WorkArea;
	bool bIsPrimary = false;
};

/**
 * Provides additional information on the display devices that is not available
 * in FDisplayMetrics.
 */
class AVALANCHEMEDIA_API FAvaDisplayDeviceManager
{
public:
	static void EnumMonitors(TArray<FAvaMonitorInfo>& OutMonitorInfo);

	static const TArray<FAvaMonitorInfo>&  GetCachedMonitors(bool bForceUpdate = false);

	/**
	 *	Returns a string with a display name for the monitor.
	 **/
	static FString GetMonitorDisplayName(const FAvaMonitorInfo& InMonitorInfo);
	
private:
	static TArray<FAvaMonitorInfo> CachedMonitorInfo;
};