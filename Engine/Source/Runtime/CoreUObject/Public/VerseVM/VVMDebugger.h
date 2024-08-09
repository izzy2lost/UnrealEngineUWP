// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMContext.h"

namespace Verse
{
struct FLocation;
struct FOp;
struct VFrame;
struct VUniqueString;

struct FDebugger
{
	virtual ~FDebugger() = default;
	virtual void Notify(FRunningContext, VFrame&, const FOp&) = 0;
	virtual void AddLocation(FAllocationContext, VUniqueString& FilePath, const FLocation&) = 0;
};

COREUOBJECT_API FDebugger* GetDebugger();

COREUOBJECT_API void SetDebugger(FDebugger*);
} // namespace Verse

#endif
