// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h"

#define VERSE_UNREACHABLE()   \
	do                        \
	{                         \
		while (true)          \
		{                     \
			PLATFORM_BREAK(); \
		}                     \
	}                         \
	while (false)
