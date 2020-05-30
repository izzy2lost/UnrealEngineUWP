// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	UWPOutputDevices.h: Windows platform OutputDevices functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FUWPOutputDevices : public FGenericPlatformOutputDevices
{
	static FOutputDevice*			GetEventLog();
};

typedef FUWPOutputDevices FPlatformOutputDevices;
