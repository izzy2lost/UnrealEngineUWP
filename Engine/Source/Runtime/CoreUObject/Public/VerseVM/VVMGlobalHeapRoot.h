// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h"

namespace Verse
{
struct FMarkStack;

// If you have a class that is meant to be used exclusively for global variables (like TUniqueConstructor), then
// subclass this to give that class the ability to mark its referenced cells.
//
// Never use this for things that aren't truly global.
struct FGlobalHeapRoot
{
	COREUOBJECT_API FGlobalHeapRoot();

	virtual void MarkReferencedCells(FMarkStack&) = 0;
};

} // namespace Verse
