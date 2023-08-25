// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"

namespace Verse
{
struct FOpResult;

using FNativeCallResult = FOpResult;

// A function that is implemented in C++
struct VNativeFunction : VHeapValue
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	// Interface between VerseVM and C++
	using FThunkFn = FNativeCallResult (*)(FRunningContext, VValue /* Argument */);

	// The C++ function to call
	FThunkFn Thunk;

	static VNativeFunction& New(FAllocationContext Context, FThunkFn Thunk)
	{
		return *new (Context.AllocateFastCell(sizeof(VNativeFunction))) VNativeFunction(Context, Thunk);
	}

private:
	VNativeFunction(FAllocationContext Context, FThunkFn InThunk)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, Thunk(InThunk)
	{
	}
};

} // namespace Verse
