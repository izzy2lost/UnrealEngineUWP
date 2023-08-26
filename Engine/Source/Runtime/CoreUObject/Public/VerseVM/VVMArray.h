// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMCell.h"
#include "VVMWriteBarrier.h"

namespace Verse
{
struct VTuple;
struct VCppClassInfo;

template <VCppClassInfo*>
struct TGlobalTrivialEmergentTypePtr;

// An Array can be extended with Add

struct VArray : VHeapValue
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

private:
	uint32 NumValues;
	TWriteBarrier<VTuple> Tuple;

public:
	VTuple& GetTuple() { return *Tuple.Get(); }

	uint32 Num() const
	{
		return NumValues;
	}

	uint32 Capacity() const;

	bool IsInBounds(const VInt& Index) const;

	void SetValue(FAccessContext Context, uint32 Index, VValue Value);
	VValue GetValue(uint32 Index);
	void AddValue(FAllocationContext Context, VValue Value);

	void Append(FAllocationContext Context, VArray& Array);

	// Capacity is initial capacity
	static VArray& New(FAllocationContext Context, uint32 InitialCapacity = 1)
	{
		return *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, InitialCapacity);
	}

	static VArray& New(FAllocationContext Context, VArray& Array)
	{
		VArray& NewArray = *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, Array.Num());
		NewArray.Append(Context, Array);
		return NewArray;
	}

	static VArray& New(FAllocationContext Context, std::initializer_list<VValue> InitList)
	{
		VArray& NewArray = *new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, static_cast<uint32>(InitList.size()));
		for (const VValue& Value : InitList)
		{
			NewArray.AddValue(Context, Value);
		}
		return NewArray;
	}

	static VArray& Concat(FAllocationContext Context, VArray& Lhs, VArray& Rhs);

	COREUOBJECT_API static void MarkReferencedCellsImpl(VCell* This, FMarkStack&);

	COREUOBJECT_API static bool EqualImpl(FRunningContext Context, VCell* This, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder);

	COREUOBJECT_API static uint32 GetTypeHashImpl(VCell* ThisCell);

	// C++ ranged-based iteration
	class FConstIterator
	{
	public:
		FORCEINLINE VValue operator*() const { return CurrentValue->Get(); }
		FORCEINLINE bool operator==(const FConstIterator& Rhs) const { return CurrentValue == Rhs.CurrentValue; }
		FORCEINLINE bool operator!=(const FConstIterator& Rhs) const { return CurrentValue != Rhs.CurrentValue; }
		FORCEINLINE FConstIterator& operator++()
		{
			++CurrentValue;
			return *this;
		}

	private:
		friend struct VArray;
		FORCEINLINE FConstIterator(const TWriteBarrier<VValue>* InCurrentValue)
			: CurrentValue(InCurrentValue) {}
		const TWriteBarrier<VValue>* CurrentValue;
	};
	COREUOBJECT_API FConstIterator begin() const;
	COREUOBJECT_API FConstIterator end() const;

private:
	COREUOBJECT_API VArray(FAllocationContext Context, uint32 InitialCapacity);
};

} // namespace Verse