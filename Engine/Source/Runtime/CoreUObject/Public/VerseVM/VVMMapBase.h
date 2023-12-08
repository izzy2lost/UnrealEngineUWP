// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

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

struct VMapBaseInternalKeyFuncs : TDefaultMapKeyFuncs<TWriteBarrier<VValue>, TWriteBarrier<VValue>, false>
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
using VMapBaseInternal = TMap<TWriteBarrier<VValue>, TWriteBarrier<VValue>, FDefaultSetAllocator, VMapBaseInternalKeyFuncs>;

struct VMapBase : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

protected:
	// TODO: Create an allocator for this map which uses the GC's Aux allocation so we don't have to count external memory
	VMapBaseInternal InternalMap;

	void Add(FAllocationContext Context, VValue Key, VValue Value);

	VMapBase(FAllocationContext Context, uint32 InitialCapacity, VEmergentType* Type)
		: VHeapValue(Context, Type)
	{
		InternalMap.Reserve(InitialCapacity);
		FHeap::ReportAllocatedNativeBytes(InternalMap.GetAllocatedSize());
	}

	// TODO: When constructing with duplicate keys, we should forget the earlier
	// key ever existed, and it shouldn't change the order.
	VMapBase(FAllocationContext Context, std::initializer_list<TPair<VValue, VValue>> InitList, VEmergentType* Type)
		: VHeapValue(Context, Type)
	{
		InternalMap.Reserve(static_cast<uint32>(InitList.size()));
		for (const TPair<VValue, VValue>& Pair : InitList)
		{
			Add(Context, Pair.Key, Pair.Value);
		}
		FHeap::ReportAllocatedNativeBytes(InternalMap.GetAllocatedSize());
	}

	template <typename InitEntryByIndex>
	VMapBase(FAllocationContext Context, uint32 NumEntries, InitEntryByIndex&& InitEntryFunc, VEmergentType* Type)
		: VHeapValue(Context, Type)
	{
		InternalMap.Reserve(NumEntries);
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			TPair<VValue, VValue> Pair = InitEntryFunc(Index);
			Add(Context, Pair.Key, Pair.Value);
		}
		FHeap::ReportAllocatedNativeBytes(InternalMap.GetAllocatedSize());
	}

	~VMapBase();

public:
	int32 Num() const
	{
		return InternalMap.Num();
	}

	VValue Find(const VValue Key);

	// GetKey/GetValue doesn't verify that Index is within limits and
	// only works as long as nothing is removed from the map.
	VValue GetKey(const int32 Index);
	VValue GetValue(const int32 Index);

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
		friend struct VMapBase;
		FORCEINLINE FConstIterator(VMapBaseInternal::TRangedForConstIterator InCurrentValue)
			: CurrentValue(InCurrentValue) {}
		VMapBaseInternal::TRangedForConstIterator CurrentValue;
	};
	FORCEINLINE FConstIterator begin() const { return InternalMap.begin(); }
	FORCEINLINE FConstIterator end() const { return InternalMap.end(); }
};

} // namespace Verse
#endif // WITH_VERSE_VM
