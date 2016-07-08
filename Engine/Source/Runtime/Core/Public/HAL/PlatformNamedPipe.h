// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformNamedPipe.h"
// @ATG_CHANGE : BEGIN UWP support
#elif PLATFORM_UWP
#include "UWP/UWPNamedPipe.h"
// @ATG_CHANGE : END
#else
#include "GenericPlatform/GenericPlatformNamedPipe.h"
#endif