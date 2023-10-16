// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"

namespace Verse
{
struct VProcedure;

struct VFunction : VCell
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;
	DECLARE_VISIT_REFERENCES(COREUOBJECT_API);

	using Args = TArray<VValue, TInlineAllocator<8>>;

	TWriteBarrier<VProcedure> Procedure;
	const uint32 NumCaptures;
	TWriteBarrier<VValue> Captures[];

	// Upon failure, returns an uninitialized VValue
	COREUOBJECT_API VValue InvokeInTransaction(FRunningContext Context, VValue Argument);
	COREUOBJECT_API VValue InvokeInTransaction(FRunningContext Context, Args&& Args);

	static VFunction& New(FAllocationContext Context, VProcedure& Procedure, uint32 NumCaptures)
	{
		return *new (Context.AllocateFastCell(offsetof(VFunction, Captures) + sizeof(Captures[0]) * NumCaptures)) VFunction(Context, Procedure, NumCaptures);
	}

	VProcedure& GetProcedure() { return *Procedure.Get(); }

private:
	VFunction(FAllocationContext Context, VProcedure& InFunction, uint32 InNumCaptures)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, Procedure(Context, &InFunction)
		, NumCaptures(InNumCaptures)
	{
		for (uint32 CaptureIndex = 0; CaptureIndex < NumCaptures; ++CaptureIndex)
		{
			new (&Captures[CaptureIndex]) TWriteBarrier<VValue>{};
		}
	}
};
} // namespace Verse
