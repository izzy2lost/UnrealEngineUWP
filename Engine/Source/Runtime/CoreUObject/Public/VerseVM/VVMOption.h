// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "VVMContext.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMValue.h"

namespace Verse
{

struct VOption : VHeapValue
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VOption& New(FAllocationContext Context, VValue InValue)
	{
		return *new (Context.AllocateFastCell(sizeof(VOption))) VOption(Context, InValue);
	}

	VValue GetValue() const
	{
		return Value.Get().Follow();
	}

	void SetValue(FRunningContext Context, VValue InValue)
	{
		Value.Set(Context, InValue);
	}

	VValue operator*()
	{
		return GetValue();
	}

	COREUOBJECT_API static void MarkReferencedCellsImpl(VCell* This, FMarkStack&);

	COREUOBJECT_API static uint32 GetTypeHashImpl(VCell* ThisCell);

private:
	VOption(FAllocationContext Context, VValue InValue)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
	{
		Value.Set(Context, InValue);
	}

	TWriteBarrier<VValue> Value;
};

} // namespace Verse