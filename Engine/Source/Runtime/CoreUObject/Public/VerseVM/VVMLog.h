// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Logging/LogMacros.h"
#include "VVMUnreachable.h"

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogVerseVM, Log, All);
COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogVerseGC, Log, All);

#define V_DIE(Format, ...)                                      \
	do                                                          \
	{                                                           \
		UE_LOG(LogVerseVM, Fatal, TEXT(Format), ##__VA_ARGS__); \
		VERSE_UNREACHABLE();                                    \
	}                                                           \
	while (false)

#define V_DIE_IF(Expression) UE_CLOG(Expression, LogVerseVM, Fatal, TEXT("Unexpected condition: " #Expression))
#define V_DIE_UNLESS(Expression) UE_CLOG(!(Expression), LogVerseVM, Fatal, TEXT("Assertion failed: " #Expression))
