// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Containers/Map.h"
#include "CoreTypes.h"
#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMHeap.h"
#include "VVMValue.h"
#include "VVMWriteBarrier.h"

namespace Verse
{

/*
 * The following types can be used as keys:
 * logic
 * int
 * float
 * char
 * string
 * enum
 * A class, if it’s comparable
 * An option, if the element type is comparable
 * An array, if the element type is comparable
 * A map if both the key and the value types are comparable
 * A tuple if all elements in the tuple are comparable
 */

/*
 * Design Notes:
 *
 * Currently we report external memory used by the VMap's TMap directly to the VM's Heap during allocation/deallocation.
 * We could instead use the TMap's functionality which allows us to define its allocator ourselves
 * and simply have the Heap allocate the maps members directly avoiding the need for us to report manually.
 *
 * This would require:
 * - Making sure non VCells can be marked and thus counted by allocation
 * - Implementing 'mark auxillary' so the GC does not free non-VCells which have been marked this way
 */

struct VMapInternalKeyFuncs : TDefaultMapKeyFuncs<TWriteBarrier<VValue>, TWriteBarrier<VValue>, false>
{
	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return VValue::Equal(FRunningContextPromise(), A.Get(), B.Get(), [](VValue Left, VValue Right) {
			checkSlow(!Left.IsPlaceholder());
			checkSlow(!Right.IsPlaceholder());
		});
	}

	static FORCEINLINE bool Matches(KeyInitType A, VValue B)
	{
		return VValue::Equal(FRunningContextPromise(), A.Get(), B, [](VValue Left, VValue Right) {
			checkSlow(!Left.IsPlaceholder());
			checkSlow(!Right.IsPlaceholder());
		});
	}

	static uint32 GetKeyHash(KeyInitType Key);
	static uint32 GetKeyHash(VValue Key);
};
using VMapInternal = TMap<TWriteBarrier<VValue>, TWriteBarrier<VValue>, FDefaultSetAllocator, VMapInternalKeyFuncs>;

struct VMap : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	VMapInternal InternalMap;

	static VMap& New(FAllocationContext Context, uint32 InitialCapacity = 0)
	{
		VMap& NewMap = *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMap))) VMap(Context, InitialCapacity);
		return NewMap;
	}

	static VMap& New(FAllocationContext Context, std::initializer_list<TPair<VValue, VValue>> InitList)
	{
		VMap& NewMap = *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMap))) VMap(Context, static_cast<uint32>(InitList.size()));
		for (const TPair<VValue, VValue>& Pair : InitList)
		{
			NewMap.Add(Context, Pair.Key, Pair.Value);
		}
		return NewMap;
	}

	int32 Num() const
	{
		return InternalMap.Num();
	}

	VValue Find(const VValue Key);

	// GetKey/GetValue doesn't verify that Index is within limits and
	// only works as long as nothing is removed from the map.
	VValue GetKey(const int32 Index);
	VValue GetValue(const int32 Index);
	void Add(FAllocationContext Context, VValue Key, VValue Value);

	size_t GetAllocatedSize() const
	{
		return InternalMap.GetAllocatedSize();
	}

	COREUOBJECT_API bool EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

	COREUOBJECT_API uint32 GetTypeHashImpl();

	// C++ ranged-based iteration
	class FConstIterator
	{
	public:
		FORCEINLINE TPair<VValue, VValue> operator*() const { return {CurrentValue->Key.Get(), CurrentValue->Value.Get()}; }
		FORCEINLINE bool operator==(const FConstIterator& Rhs) const { return CurrentValue == Rhs.CurrentValue; }
		FORCEINLINE bool operator!=(const FConstIterator& Rhs) const { return CurrentValue != Rhs.CurrentValue; }
		FORCEINLINE FConstIterator& operator++()
		{
			++CurrentValue;
			return *this;
		}

	private:
		friend struct VMap;
		FORCEINLINE FConstIterator(VMapInternal::TRangedForConstIterator InCurrentValue)
			: CurrentValue(InCurrentValue) {}
		VMapInternal::TRangedForConstIterator CurrentValue;
	};
	FORCEINLINE FConstIterator begin() const { return InternalMap.begin(); }
	FORCEINLINE FConstIterator end() const { return InternalMap.end(); }

private:
	VMap(FAllocationContext Context, uint32 InitialCapacity)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
	{
		InternalMap.Reserve(InitialCapacity);
		FHeap::ReportAllocatedNativeBytes(InternalMap.GetAllocatedSize());
	}

	~VMap();
};

} // namespace Verse