// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h"

namespace Verse
{
struct FMarkStack;

// If you have a class that is meant to be used exclusively globally and has weak references, then
// subclass this to give that class the ability to conduct census and clear those references.
//
// Never use this for things that aren't truly global.
//
// It's fine to have classes subclass both FGLobalHeapRoot (for strong references) and
// FGLobalHeapCensusRoot (for weak references).
struct FGlobalHeapCensusRoot
{
	COREUOBJECT_API FGlobalHeapCensusRoot();

	virtual void ConductCensus() = 0;
};

} // namespace Verse
