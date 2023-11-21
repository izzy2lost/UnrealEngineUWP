// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Containers/StringView.h"
#include "VerseVM/Inline/VVMMutableArrayInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMUTF8String.h"

namespace Verse
{
struct VPackage : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	// We keep names at 2*Index and definitions at 2*Index+1
	TWriteBarrier<VMutableArray> NameAndDefinitions;

	uint32 Num() const
	{
		return NameAndDefinitions->Num() / 2;
	}

	const VUTF8String& GetName(FAllocationContext Context, uint32 Index) const
	{
		checkSlow(Index < static_cast<int32>(Num()));
		VValue Value = NameAndDefinitions->GetValue(2 * Index);
		check(Value.IsCell());
		check(Value.AsCell().IsA<VUTF8String>());
		return Value.AsCell().StaticCast<VUTF8String>();
	}

	VValue GetDefinition(FAllocationContext Context, uint32 Index) const
	{
		checkSlow(Index < static_cast<int32>(Num()));
		return NameAndDefinitions->GetValue(2 * Index + 1);
	}

	void AddDefinition(FAllocationContext Context, FUtf8StringView Name, VValue Definition)
	{
		NameAndDefinitions->AddValue(Context, VValue(VUTF8String::New(Context, Name)));
		NameAndDefinitions->AddValue(Context, Definition);
	}

	void AddDefinition(FAllocationContext Context, VUTF8String& Name, VValue Definition)
	{
		NameAndDefinitions->AddValue(Context, VValue(Name));
		NameAndDefinitions->AddValue(Context, Definition);
	}

	VValue Lookup(FAllocationContext Context, FUtf8StringView Name) const
	{
		for (uint32 Index = 0, End = Num(); Index < End; ++Index)
		{
			if (GetName(Context, Index).Equals(Name))
			{
				return GetDefinition(Context, Index);
			}
		}
		return VValue();
	}

	template <typename CellType>
	CellType* LookupCell(FAllocationContext Context, FUtf8StringView Name) const
	{
		VValue Value = Lookup(Context, Name);
		if (Value.IsCell())
		{
			VCell& Cell = Value.AsCell();
			if (Cell.IsA<CellType>())
			{
				return &Cell.StaticCast<CellType>();
			}
		}
		return nullptr;
	}

	static VPackage& New(FAllocationContext Context, uint32 Capacity)
	{
		return *new (Context.AllocateFastCell(sizeof(VPackage))) VPackage(Context, Capacity);
	}

private:
	VPackage(FAllocationContext Context, uint32 Capacity)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NameAndDefinitions(Context, &VMutableArray::New(Context, Capacity))
	{
	}
};
} // namespace Verse
