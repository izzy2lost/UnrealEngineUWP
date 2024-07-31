// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMDebugger.h"

static Verse::FDebugger* Debugger = nullptr;

Verse::FDebugger* Verse::GetDebugger()
{
	return Debugger;
}

void Verse::SetDebugger(FDebugger* Arg)
{
	Debugger = Arg;
}

#endif
