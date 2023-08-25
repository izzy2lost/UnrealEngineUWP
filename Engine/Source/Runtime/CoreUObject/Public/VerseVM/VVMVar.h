// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMRestValue.h"
#include "VVMType.h"

namespace Verse
{

struct VVar : VCell
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VVar& New(FAllocationContext Context)
	{
		return *new (Context.AllocateFastCell(sizeof(VVar))) VVar(Context);
	}

	VValue Get(FAllocationContext Context)
	{
		return Value.Get(Context);
	}

	void Set(FAccessContext Context, VValue NewValue);

	void SetNonTransactionally(FAccessContext Context, VValue NewValue)
	{
		return Value.Set(Context, NewValue);
	}

	COREUOBJECT_API static void MarkReferencedCellsImpl(VCell* This, FMarkStack&);

private:
	VRestValue Value;

	VVar(FAllocationContext Context)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		// TODO: Figure out what split depth meets here.
		, Value(0)
	{
	}
};

} // namespace Verse
