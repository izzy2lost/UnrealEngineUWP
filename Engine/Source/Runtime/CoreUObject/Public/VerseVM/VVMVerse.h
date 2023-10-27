// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

namespace Verse
{
class VerseVM
{
public:
	COREUOBJECT_API static void Startup();
	COREUOBJECT_API static void Shutdown();
};
} // namespace Verse
