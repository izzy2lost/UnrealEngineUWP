// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	UWPString.h: UWP platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once
#include "GenericPlatform/MicrosoftPlatformString.h"
#include "HAL/Platform.h"

/**
 * UWP string implementation
 */
struct FUWPString : public FMicrosoftPlatformString
{
};

typedef FUWPString FPlatformString;
