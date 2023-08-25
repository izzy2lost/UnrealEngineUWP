// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMCell.h"
#include "VVMGlobalHeapPtr.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMOption.h"

namespace Verse
{

struct VFalse : VCell
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static void InitializeGlobals();

private:
	VFalse(FAllocationContext Context)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	{
	}

	static VFalse& New(FAllocationContext Context)
	{
		return *new (Context.AllocateFastCell(sizeof(VFalse))) VFalse(Context);
	}
};

COREUOBJECT_API extern TGlobalHeapPtr<VFalse> GlobalFalsePtr;
COREUOBJECT_API extern TGlobalHeapPtr<VOption> GlobalTruePtr;

// True is represented as VOption(VFalse)
static VOption& GlobalTrue()
{
	return *GlobalTruePtr.Get();
}

static VFalse& GlobalFalse()
{
	return *GlobalFalsePtr.Get();
}

} // namespace Verse
