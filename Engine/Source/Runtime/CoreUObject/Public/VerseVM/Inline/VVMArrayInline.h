// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VerseVM/Inline/VVMTupleInline.h"
#include "VerseVM/VVMArray.h"

namespace Verse
{
inline uint32 VArray::Capacity() const
{
	return Tuple->Num();
}

inline bool VArray::IsInBounds(const VInt& Index) const
{
	return Tuple->IsInBounds(Index);
}

inline void VArray::SetValue(FAccessContext Context, uint32 Index, VValue Value)
{
	checkSlow(Index < NumValues);
	Tuple->SetValue(Context, Index, Value);
}

inline VValue VArray::GetValue(uint32 Index)
{
	checkSlow(Index < NumValues);
	return Tuple->GetValue(Index);
}

inline void VArray::AddValue(FAllocationContext Context, VValue Value)
{
	uint32 CurrentCapacity = Capacity();
	if (NumValues == CurrentCapacity)
	{
		uint32 NewCapacity = 2 * (CurrentCapacity ? CurrentCapacity : 2);
		VTuple& NewTuple = VTuple::New(Context, NewCapacity);
		VTuple& OldTuple = *Tuple;
		for (uint32 Index = 0; Index < CurrentCapacity; ++Index)
		{
			NewTuple.SetValue(Context, Index, OldTuple.GetValue(Index));
		}
		Tuple.Set(Context, &NewTuple);
	}
	Tuple->SetValue(Context, NumValues, Value);
	++NumValues;
}

inline void VArray::Append(FAllocationContext Context, VArray& Array)
{
	const uint32 ArrayNum = Array.Num();
	if (ArrayNum == 0)
	{
		return;
	}
	for (uint32 Index = 0; Index < ArrayNum; ++Index)
	{
		AddValue(Context, Array.GetValue(Index));
	}
}

inline VArray& VArray::Concat(FAllocationContext Context, VArray& Lhs, VArray& Rhs)
{
	VArray* NewArray = new (Context.AllocateFastCell(sizeof(VArray))) VArray(Context, Lhs.Num() + Rhs.Num());
	NewArray->Append(Context, Lhs);
	NewArray->Append(Context, Rhs);
	return *NewArray;
}
} // namespace Verse