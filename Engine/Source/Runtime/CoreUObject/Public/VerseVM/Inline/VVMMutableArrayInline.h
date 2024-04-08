// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMutableArray.h"

namespace Verse
{

inline void VMutableArray::AddValue(FAllocationContext Context, VValue Value)
{
	if (!GetData())
	{
		Capacity = 4;
		AllocateBuffer(Context, DetermineArrayType(Value), Capacity);
	}
	else if (GetArrayType() != EArrayType::VValue && GetArrayType() != DetermineArrayType(Value))
	{
		if (Num() == Capacity) // Check our capacity before re-allocating as VValues
		{
			Capacity = Capacity * 2;
		}
		ConvertDataToVValues(Context, &Capacity);
	}
	else if (Num() == Capacity)
	{
		Capacity = Capacity * 2;
		TAux<void> NewValues(Context.AllocateAuxCell(ByteLength(GetArrayType()) * Capacity));
		FMemory::Memcpy(NewValues.GetPtr(), Values.Get().GetPtr(), ByteLength());
		Values.Set(Context, BitCast<TAux<void>>(NewValues));
	}

	uint32 Index = Num();
	++NumValues;
	SetValue(Context, Index, Value);
	if (IsString())
	{
		SetNullTerminator();
	}
}

template <typename T>
inline void VMutableArray::Append(FAllocationContext Context, VArrayBase& Array)
{
	checkSlow(GetArrayType() != EArrayType::VValue && GetArrayType() == Array.GetArrayType());
	const uint32 NewNumValues = Num() + Array.Num();
	if (NewNumValues > Capacity)
	{
		Capacity = NewNumValues * 2;
		TAux<void> NewValues(Context.AllocateAuxCell(sizeof(T) * Capacity));
		FMemory::Memcpy(NewValues.GetPtr(), GetData(), ByteLength());
		Values.Set(Context, NewValues);
	}
	FMemory::Memcpy(GetData<T>() + Num(), Array.GetData<T>(), Array.ByteLength());
	NumValues = NewNumValues;
	if (IsString())
	{
		SetNullTerminator();
	}
}

template <>
inline void VMutableArray::Append<TWriteBarrier<VValue>>(FAllocationContext Context, VArrayBase& Array)
{
	checkSlow(GetArrayType() == EArrayType::VValue);
	for (uint32 Index = 0, End = Array.Num(); Index < End; ++Index)
	{
		AddValue(Context, Array.GetValue(Index));
	}
}

inline VMutableArray& VMutableArray::Concat(FAllocationContext Context, VArrayBase& Lhs, VArrayBase& Rhs)
{
	VMutableArray& NewArray = VMutableArray::New(Context, Lhs.Num() + Rhs.Num(), DetermineCombinedType(Lhs.GetArrayType(), Rhs.GetArrayType()));
	NewArray.Append(Context, Lhs);
	NewArray.Append(Context, Rhs);
	return NewArray;
}

} // namespace Verse
#endif // WITH_VERSE_VM
