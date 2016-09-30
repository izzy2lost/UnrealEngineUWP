// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformTime.h"
#elif PLATFORM_PS4
#include "PS4/PS4Time.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneTime.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformTime.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformTime.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidTime.h"
// @ATG_CHANGE : BEGIN UWP support
#elif PLATFORM_UWP
#include "UWP/UWPTime.h"
// @ATG_CHANGE : END
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformTime.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformTime.h"
#endif