// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UWP/UWPPlatformSplash.h"
#include "UWP/UWPPlatformApplicationMisc.h"

void FUWPSplash::Show()
{
	//@todo.UWP: Implement me
	FUWPPlatformApplicationMisc::PumpMessages(true);
}

void FUWPSplash::Hide()
{
	//@todo.UWP: Implement me
	FUWPPlatformApplicationMisc::PumpMessages(true);
}

void FUWPSplash::SetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
{
	//@todo.UWP: Implement me
}
