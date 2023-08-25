// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h"

class FString;

namespace Verse
{
struct FOp;
struct FRunningContext;
struct VProcedure;

COREUOBJECT_API FString PrintProcedure(FRunningContext, VProcedure& Function);

} // namespace Verse
