// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMCell.h"
#include "VerseVM/VVMNameValueMap.h"

namespace Verse
{
enum class EDigestVariant : uint8
{
	PublicAndEpicInternal = 0,
	PublicOnly = 1,
};

struct VPackage : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VUTF8String> DigestCode[2]; // One for each variant

	uint32 Num() const { return Map.Num(); }
	const VUTF8String& GetName(uint32 Index) const { return Map.GetName(Index); }
	VValue GetDefinition(uint32 Index) const { return Map.GetValue(Index); }
	void AddDefinition(FAllocationContext Context, FUtf8StringView Name, VValue Definition) { Map.AddValue(Context, Name, Definition); }
	VValue LookupDefinition(FUtf8StringView Name) const { return Map.Lookup(Name); }
	template <typename CellType>
	CellType* LookupDefinition(FUtf8StringView Name) const { return Map.LookupCell<CellType>(Name); }

	static VPackage& New(FAllocationContext Context, uint32 Capacity)
	{
		return *new (Context.AllocateFastCell(sizeof(VPackage))) VPackage(Context, Capacity);
	}

private:
	VPackage(FAllocationContext Context, uint32 Capacity)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, Map(Context, Capacity)
	{
	}

	VNameValueMap Map;
};
} // namespace Verse
