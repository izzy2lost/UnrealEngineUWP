// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMTuple.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
inline void VTuple::SetValue(FAccessContext Context, uint32 Index, VValue Value)
{
	checkSlow(Index < NumValues);
	Values[Index].Set(Context, Value);
}

inline VValue VTuple::GetValue(uint32 Index)
{
	checkSlow(Index < NumValues);
	return Values[Index].Follow();
}

inline bool VTuple::IsInBounds(const VInt& Index) const
{
	if (Index.IsInt64())
	{
		const int64 IndexInt64 = Index.AsInt64();
		return (IndexInt64 >= 0) && (IndexInt64 < NumValues);
	}
	else
	{
		// Array/tuple maximum size is limited to the maximum size of a unsigned 32-bit integer.
		// So even if it's a `VHeapInt`, if it fails the `IsInt64` check, it is definitely out-of-bounds.
		return false;
	}
}
} // namespace Verse
