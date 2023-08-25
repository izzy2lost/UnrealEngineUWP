// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h"

namespace Verse
{

// This is useful for telling MSVC to cooperate.
COREUOBJECT_API extern bool bGTrue;

} // namespace Verse
