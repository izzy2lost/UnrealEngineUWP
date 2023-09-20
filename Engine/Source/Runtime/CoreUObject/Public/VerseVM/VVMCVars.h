// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/IConsoleManager.h"

namespace Verse
{
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarTraceExecution;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarSingleStepTraceExecution;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarDumpBytecode;
extern COREUOBJECT_API TAutoConsoleVariable<bool> CVarForceBytecode;
} // namespace Verse
