// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
	#include "Windows/WindowsHWrapper.h" // HEADER_UNIT_IGNORE
#else
	#include "CoreTypes.h"
	#include "HAL/PlatformMemory.h"
	#include "Microsoft/PreWindowsApiPrivate.h"
	#ifndef STRICT
	#define STRICT
	#endif
	#include "Microsoft/MinWindowsPrivate.h"
	#include "Microsoft/PostWindowsApiPrivate.h"
#endif
