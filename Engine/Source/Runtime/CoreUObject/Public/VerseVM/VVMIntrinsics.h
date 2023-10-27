// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMNativeFunction.h"
#include "VVMType.h"

namespace Verse
{

// A special heap value to store all intrinsic VNativeFunction objects
struct VIntrinsics : VHeapValue
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VNativeFunction> Abs;
	TWriteBarrier<VNativeFunction> Ceil;
	TWriteBarrier<VNativeFunction> Floor;

	static VIntrinsics& New(FAllocationContext Context)
	{
		return *new (Context.AllocateFastCell(sizeof(VIntrinsics))) VIntrinsics(Context);
	}

private:
	static FNativeCallResult AbsImpl(FRunningContext Context, VNativeFunction::Args Arguments);
	static FNativeCallResult CeilImpl(FRunningContext Context, VNativeFunction::Args Arguments);
	static FNativeCallResult FloorImpl(FRunningContext Context, VNativeFunction::Args Arguments);

	VIntrinsics(FAllocationContext Context)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, Abs(Context, VNativeFunction::New(Context, 1, &AbsImpl))
		, Ceil(Context, VNativeFunction::New(Context, 1, &CeilImpl))
		, Floor(Context, VNativeFunction::New(Context, 1, &FloorImpl))
	{
	}
};

} // namespace Verse