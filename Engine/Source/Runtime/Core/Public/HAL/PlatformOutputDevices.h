// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformOutputDevices.h"
#elif PLATFORM_PS4
#include "PS4/PS4OutputDevices.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneOutputDevices.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformOutputDevices.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformOutputDevices.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidOutputDevices.h"
// @ATG_CHANGE : BEGIN UWP support
#elif PLATFORM_UWP
#include "UWP/UWPOutputDevices.h"
// @ATG_CHANGE : END
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformOutputDevices.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformOutputDevices.h"
#endif
