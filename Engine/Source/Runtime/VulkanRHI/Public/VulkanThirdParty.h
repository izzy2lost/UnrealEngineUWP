// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	#include "Microsoft/AllowMicrosoftPlatformTypes.h"
	#include <windows.h>
	#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
	#define VK_NO_PROTOTYPES
	#include <vulkan.h>
THIRD_PARTY_INCLUDES_END
