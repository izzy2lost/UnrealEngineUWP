// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMArray.h"
#include "VVMArrayBase.h"
#include "VVMEmergentTypeCreator.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"

namespace Verse
{

struct VMutableArray : VArrayBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VArrayBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

private:
	uint32 Capacity;

public:
	void SetValue(FAccessContext Context, uint32 Index, VValue Value);
	void AddValue(FAllocationContext Context, VValue Value);
	void Append(FAllocationContext Context, VArrayBase& Array);

	VArray& AsArray(FRunningContext Context)
	{
		VArray& Result = VArray::New(Context, 0);
		Result.NumValues = NumValues;
		Result.Values.Set(Context, Values.Get());
		return Result;
	}

	static VMutableArray& New(FAllocationContext Context, uint32 InitialCapacity = 1)
	{
		return *new (Context.AllocateFastCell(sizeof(VMutableArray))) VMutableArray(Context, InitialCapacity);
	}

	static VMutableArray& New(FAllocationContext Context, std::initializer_list<VValue> InitList)
	{
		return *new (Context.AllocateFastCell(sizeof(VMutableArray))) VMutableArray(Context, InitList);
	}

	template <typename InitIndexFunc>
	static VMutableArray& New(FAllocationContext Context, uint32 InNumValues, InitIndexFunc&& InitFunc)
	{
		return *new (Context.AllocateFastCell(sizeof(VMutableArray))) VMutableArray(Context, InNumValues, InitFunc);
	}

	static VMutableArray& New(FAllocationContext Context, VMutableArray& Other)
	{
		return *new (Context.AllocateFastCell(sizeof(VMutableArray))) VMutableArray(Context, Other);
	}

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

private:
	VMutableArray(FAllocationContext Context, uint32 InitialCapacity)
		: VArrayBase(Context, InitialCapacity, &GlobalTrivialEmergentType.Get(Context))
		, Capacity(InitialCapacity)
	{
		NumValues = 0;
	}

	VMutableArray(FAllocationContext Context, std::initializer_list<VValue> InitList)
		: VArrayBase(Context, InitList, &GlobalTrivialEmergentType.Get(Context))
		, Capacity(static_cast<uint32>(InitList.size())) {}

	template <typename InitIndexFunc>
	VMutableArray(FAllocationContext Context, uint32 InNumValues, InitIndexFunc&& InitFunc)
		: VArrayBase(Context, InNumValues, InitFunc, &GlobalTrivialEmergentType.Get(Context))
		, Capacity(InNumValues) {}

	VMutableArray(FAllocationContext Context, VMutableArray& Other)
		: VArrayBase(Context, Other)
		, Capacity(Other.Capacity) {}
};

} // namespace Verse