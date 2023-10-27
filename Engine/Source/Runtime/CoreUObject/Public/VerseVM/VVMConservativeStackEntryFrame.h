// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

namespace Verse
{
struct FConservativeStackExitFrame;

struct FConservativeStackEntryFrame
{
	FConservativeStackExitFrame* ExitFrame;
};

} // namespace Verse
