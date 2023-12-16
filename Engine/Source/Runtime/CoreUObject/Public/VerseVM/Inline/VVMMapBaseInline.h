// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMMapBase.h"

namespace Verse
{

template <typename GetEntryByIndex>
inline VMapBase::VMapBase(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry, VEmergentType* Type)
	: VHeapValue(Context, Type)
{
	VMapBaseInternal Map;
	Map.Reserve(MaxNumEntries);

	bool bHasDuplicates = false;
	for (uint32 Index = 0; Index < MaxNumEntries; ++Index)
	{
		VValue Key;
		VValue Value;
		TPair<VValue, VValue> Pair = GetEntry(Index);
		Key = Pair.Get<0>();
		Value = Pair.Get<1>();

		TWriteBarrier<VValue>& ExistingEntry = Map.FindOrAdd(TWriteBarrier<VValue>{Context, Key}, TWriteBarrier<VValue>{});
		if (ExistingEntry)
		{
			bHasDuplicates = true;
			break;
		}

		ExistingEntry.Set(Context, Value);
	}

	// Constructing a map in Verse has these semantics:
	// - If the same key appears more than once, it's as if only the last key was provided.
	// - The order of the map is based on the textual order a map is written in.
	// - E.g, map{K1=>V1, K2=>V2} has the order (K1, V1) then (K2, V2).
	//   And map{K1=>V1, K2=>V2, K1=>V3} has the order (K2, V2) then (K1, V3).
	// The code below achieves these semantics. Surely it can be more optimized than it is now:
	// - We do repetitive hashing
	// - We do repetitive equality checks

	if (bHasDuplicates)
	{
		Map = VMapBaseInternal{};
		Map.Reserve(MaxNumEntries);

		struct CountsKeyFuncs : TDefaultMapKeyFuncs<VValue, unsigned, false>
		{
			static bool Matches(VValue A, VValue B)
			{
				return VValue::Equal(FRunningContextPromise(), A, B, [](VValue Left, VValue Right) {
					checkSlow(!Left.IsPlaceholder());
					checkSlow(!Right.IsPlaceholder());
				});
			}

			static uint32 GetKeyHash(VValue Key)
			{
				return GetTypeHash(Key);
			}
		};

		TMap<VValue, uint32, FDefaultSetAllocator, CountsKeyFuncs> Counts;
		Counts.Reserve(MaxNumEntries);
		for (uint32 Index = 0; Index < MaxNumEntries; ++Index)
		{
			VValue Key;
			VValue Value;
			TPair<VValue, VValue> Pair = GetEntry(Index);
			Key = Pair.Get<0>();
			Value = Pair.Get<1>();

			uint32& Count = Counts.FindOrAdd(Key, 0);
			++Count;
		}

		TMap<VValue, uint32, FDefaultSetAllocator, CountsKeyFuncs> Seen;
		Seen.Reserve(MaxNumEntries);
		for (uint32 Index = 0; Index < MaxNumEntries; ++Index)
		{
			VValue Key;
			VValue Value;
			TPair<VValue, VValue> Pair = GetEntry(Index);
			Key = Pair.Get<0>();
			Value = Pair.Get<1>();

			uint32 Hash = GetTypeHash(Key);

			uint32 TargetCount = *Counts.FindByHash(Hash, Key);
			uint32& CurrentCount = Seen.FindOrAddByHash(Hash, Key, 0);
			++CurrentCount;
			checkSlow(TargetCount > 0 && CurrentCount <= TargetCount);
			if (CurrentCount == TargetCount)
			{
				Map.AddByHash(Hash, TWriteBarrier<VValue>{Context, Key}, TWriteBarrier<VValue>{Context, Value});
			}
		}
	}

	// We don't need to grab the lock here because we can't be scanned by the GC yet.
	InternalMap = MoveTemp(Map);
	FHeap::ReportAllocatedNativeBytes(InternalMap.GetAllocatedSize());
}

} // namespace Verse
#endif // WITH_VERSE_VM
