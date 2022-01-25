// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	UWPProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformProperties.h"
#include "HAL/Platform.h"

struct FUWPPlatformProperties : public FGenericPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "UWP";
	}
	static FORCEINLINE const char* IniPlatformName()
	{
		return "UWP";
	}
	static FORCEINLINE const TCHAR* GetRuntimeSettingsClassName()
	{
		return TEXT("/Script/UWPRuntimeSettings.UWPRuntimeSettings");
	}
	static FORCEINLINE bool HasEditorOnlyData()
	{
		return false;
	}              
	static FORCEINLINE bool SupportsTessellation()
	{
		return true;
	}
	static FORCEINLINE bool SupportsWindowedMode()
	{
		return true;
	}
	static FORCEINLINE bool RequiresCookedData()
	{
		return true;
	}
	static FORCEINLINE bool SupportsGrayscaleSRGB()
	{
		return false; // Requires expand from G8 to RGBA
	}

	static FORCEINLINE bool HasFixedResolution()
	{
		return false;
	}
	
	static FORCEINLINE bool SupportsQuit()
	{
		return true;
	}
};

#ifdef PROPERTY_HEADER_SHOULD_DEFINE_TYPE
typedef FUWPPlatformProperties FPlatformProperties;
#endif
