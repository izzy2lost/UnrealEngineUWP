// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMArrayBase.h"
#include "VVMEmergentTypeCreator.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"
#include "VVMTypeCreator.h"
#include "VVMUniqueCreator.h"

namespace Verse
{

struct VInt;

// Array, fix number of elements, each with its own type
// No type information for the parts here.
struct VTypeArray : VType
{
	static constexpr EVerseTypeTag Tag = EVerseTypeTag::Array;
	uint32 Size;

	static VTypeArray* New(FAllocationContext Context, uint32 S)
	{
		return new (Context.AllocateFastCell(sizeof(VTypeArray))) VTypeArray(Context, S);
	}
	static bool Equals(const VType& Type, uint32 S)
	{
		if (Type.IsA<VTypeArray>())
		{
			const VTypeArray& Other = Type.StaticCast<VTypeArray>();
			return Other.Size == S;
		}
		return false;
	}

	uint32 Num() const
	{
		return Size;
	}

private:
	explicit VTypeArray(FAllocationContext& Context, uint32 S)
		: VType(Context, Tag)
		, Size(S)
	{
	}
};

struct VArray : VArrayBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VArrayBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VArray& New(FAllocationContext Context, uint32 NumValues)
	{
		return *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, NumValues);
	}

	static VArray& New(FAllocationContext Context, std::initializer_list<VValue> InitList)
	{
		return *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, InitList);
	}

	template <typename InitIndexFunc>
	static VArray& New(FAllocationContext Context, uint32 NumValues, InitIndexFunc&& InitFunc)
	{
		return *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, NumValues, InitFunc);
	}

	static VArray& New(FAllocationContext Context, VArray& Other)
	{
		return *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, Other);
	}

private:
	friend struct VMutableArray;
	VArray(FAllocationContext Context, uint32 InNumValues)
		: VArrayBase(Context, InNumValues, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeArray>(Context, InNumValues), &StaticCppClassInfo)) {}

	VArray(FAllocationContext Context, std::initializer_list<VValue> InitList)
		: VArrayBase(Context, InitList, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeArray>(Context, static_cast<uint32>(InitList.size())), &StaticCppClassInfo)) {}

	template <typename InitIndexFunc>
	VArray(FAllocationContext Context, uint32 InNumValues, InitIndexFunc&& InitFunc)
		: VArrayBase(Context, InNumValues, InitFunc, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeArray>(Context, InNumValues), &StaticCppClassInfo)) {}

	VArray(FAllocationContext Context, VArray& Other)
		: VArrayBase(Context, Other) {}
};

} // namespace Verse