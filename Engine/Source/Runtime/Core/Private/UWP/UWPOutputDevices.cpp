// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UWPOutputDevices.cpp: UWP implementations of OutputDevices functions
=============================================================================*/

#include "UWP/UWPPlatformOutputDevices.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"

#include "HAL/FeedbackContextAnsi.h"
#include "UWP/UWPOutputDevicesPrivate.h"

DEFINE_LOG_CATEGORY(LogUWPOutputDevices);

//////////////////////////////////
// FUWPOutputDevices
//////////////////////////////////
class FOutputDevice* FUWPOutputDevices::GetEventLog()
{
	static FOutputDeviceEventLog Singleton;
	return &Singleton;
}