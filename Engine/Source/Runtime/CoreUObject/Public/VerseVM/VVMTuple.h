// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMEmergentTypeCreator.h"
#include "VVMType.h"
#include "VVMTypeCreator.h"
#include "VVMUniqueCreator.h"

namespace Verse
{
struct VInt;

// Tuple, fix number of elements, each with its own type
// No type information for the parts here.
struct VTypeTuple : VType
{
	static constexpr EVerseTypeTag Tag = EVerseTypeTag::Tuple;
	uint32 Size;

	static VTypeTuple* New(FAllocationContext Context, uint32 S)
	{
		return new (Context.AllocateFastCell(sizeof(VTypeTuple))) VTypeTuple(Context, S);
	}
	static bool Equals(const VType& Type, uint32 S)
	{
		if (Type.IsA<VTypeTuple>())
		{
			const VTypeTuple& Other = Type.StaticCast<VTypeTuple>();
			return Other.Size == S;
		}
		return false;
	}

	uint32 Num() const
	{
		return Size;
	}

private:
	explicit VTypeTuple(FAllocationContext& Context, uint32 S)
		: VType(Context, Tag)
		, Size(S)
	{
	}
};

struct VTuple : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);

	const uint32 NumValues;
	TWriteBarrier<VValue> Values[];

	void SetValue(FAccessContext Context, uint32 Index, VValue Value);

	VValue GetValue(uint32 Index);

	uint32 Num() const
	{
		return NumValues;
	}

	bool IsInBounds(uint32 Index) const;
	bool IsInBounds(const VInt& Index) const;

	static VTuple& New(FAllocationContext Context, uint32 NumValues)
	{
		const size_t NumBytes = offsetof(VTuple, Values)
							  + sizeof(Values[0]) * NumValues;
		return *new (Context.AllocateFastCell(NumBytes)) VTuple(Context, NumValues);
	}

	static VTuple& New(FAllocationContext Context, std::initializer_list<VValue> InitList)
	{
		VTuple& Result = New(Context, static_cast<uint32>(InitList.size()));
		uint32 Index = 0;
		for (const VValue& Value : InitList)
		{
			Result.Values[Index++].Set(Context, Value);
		}
		return Result;
	}

	COREUOBJECT_API bool EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(VValue, VValue)>& HandlePlaceholder);

	COREUOBJECT_API uint32 GetTypeHashImpl();

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

private:
	VTuple(FAllocationContext Context, uint32 InNumValues)
		: VHeapValue(Context, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeTuple>(Context, InNumValues), &StaticCppClassInfo))
		, NumValues(InNumValues)
	{
		for (uint32 Index = 0; Index < NumValues; ++Index)
		{
			new (&Values[Index]) TWriteBarrier<VValue>();
		}
	}
};

} // namespace Verse