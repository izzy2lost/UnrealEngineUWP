// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMemory.h"


#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMemory.h"
#elif PLATFORM_PS4

#if USE_NEW_PS4_MEMORY_SYSTEM
#include "PS4/PS4Memory2.h"
#else
#include "PS4/PS4Memory.h"
#endif

#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneMemory.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformMemory.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformMemory.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidMemory.h"
// @ATG_CHANGE : BEGIN UWP support
#elif PLATFORM_UWP
#include "UWP/UWPMemory.h"
// @ATG_CHANGE : END
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformMemory.h"
#elif PLATFORM_UNIX
#include "Unix/UnixPlatformMemory.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformMemory.h"
#else
#error Unknown platform
#endif

#ifndef ENABLE_MEMORY_SCOPE_STATS
#define ENABLE_MEMORY_SCOPE_STATS 0
#endif

// This will grab the memory stats of VM and Physcial before and at the end of scope
// reporting +/- difference in memory.
// WARNING This will also capture differences in Threads which have nothing to do with the scope
#if ENABLE_MEMORY_SCOPE_STATS
class CORE_API FScopedMemoryStats
{
public:
	explicit FScopedMemoryStats(const TCHAR* Name);

	~FScopedMemoryStats();

private:
	const TCHAR* Text;
	const FPlatformMemoryStats StartStats;
};
#else
class FScopedMemoryStats
{
public:
	explicit FScopedMemoryStats(const TCHAR* Name) {}
};
#endif
