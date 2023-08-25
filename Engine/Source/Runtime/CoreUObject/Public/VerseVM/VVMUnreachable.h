// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/PlatformMisc.h"

#define VERSE_UNREACHABLE()   \
	do                        \
	{                         \
		while (true)          \
		{                     \
			UE_DEBUG_BREAK(); \
		}                     \
	}                         \
	while (false)
