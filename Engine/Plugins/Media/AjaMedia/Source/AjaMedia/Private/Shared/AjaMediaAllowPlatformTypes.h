// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#ifndef AJA_PLATFORM_TYPES_GUARD
	#define AJA_PLATFORM_TYPES_GUARD
#else
	#error Nesting AjaAllowPlatformTypes.h is not allowed!
#endif

#if !defined(PLATFORM_WINDOWS) && !defined(PLATFORM_UWP)
	#include "Processing.AJA.compat.h"
#endif

#define DWORD ::DWORD
#define FLOAT ::FLOAT

#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif
