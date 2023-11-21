// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMValuePrinting.h"
#include "Containers/UnrealString.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMRational.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMVar.h"
#include <inttypes.h>

namespace Verse
{
FString FDefaultCellFormatter::ToString(FAllocationContext Context, VCell& Cell) const
{
	TStringBuilder<128> Builder;
	Append(Builder, Context, Cell);
	return Builder.ToString();
}

bool FDefaultCellFormatter::TryAppend(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const
{
	// Logical values are handled via two globally unique cells.
	if (VValue Logic(Cell); Logic.IsLogic())
	{
		Builder << (Logic.AsBool() ? TEXT("true") : TEXT("false"));
		return true;
	}

	const VCppClassInfo* ClassInfo = Cell.GetCppClassInfo();
	if (ClassInfo != nullptr && ClassInfo->ToString != nullptr)
	{
		ClassInfo->ToString(&Cell, Builder, Context, *this);
		return true;
	}

	return false;
}

void FDefaultCellFormatter::Append(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const
{
	if (!TryAppend(Builder, Context, Cell))
	{
		Builder.Append(*Cell.DebugName());
		Builder.Append(TEXT("(, address 0x"));
		Builder.Appendf(TEXT("%p"), &Cell);
		Builder.Append(TEXT(")"));
	}
	else if (Cell.IsA<VUniqueString>() || Cell.IsA<VUniqueStringSet>())
	{
		Builder.Append(TEXT(", address 0x"));
		Builder.Appendf(TEXT("%p"), &Cell);
		Builder.Append(TEXT(")"));
	}
}

FString ToString(const VInt& Int)
{
	if (Int.IsInt64())
	{
		const int64 Int64 = Int.AsInt64();
		return LexToString(Int64);
	}
	else
	{
		V_DIE("Arbitrary-precision integers are not yet supported.");
	}
}

FString ToString(FAllocationContext Context, const FCellFormatter& Formatter, const VValue& Value)
{
	return Value.ToString(Context, Formatter);
}

void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter, const VValue& Value)
{
	return Value.ToString(Builder, Context, Formatter);
}

FString VValue::ToString(FAllocationContext Context, const FCellFormatter& Formatter) const
{
	TStringBuilder<128> Builder;
	ToString(Builder, Context, Formatter);
	return Builder.ToString();
}

void VValue::ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter) const
{

	if (*this == VValue::EffectDoneMarker())
	{
		Builder.Appendf(TEXT("0x%x"), AsInt32());
	}
	else if (IsInt())
	{
		if (IsCellOfType<VHeapInt>())
		{
			// larger heap-ints
			Formatter.Append(Builder, Context, AsCell());
		}
		else
		{
			// Smaller ints
			Builder.Append(::Verse::ToString(AsInt()));
		}
	}
	else if (IsFloat())
	{
		Builder << AsFloat().AsDouble();
	}
	else if (IsCell())
	{
		Formatter.Append(Builder, Context, AsCell());
	}
	else if (IsRoot())
	{
		Builder.Appendf(TEXT("Root(%u)"), GetSplitDepth());
	}
	else if (IsPlaceholder())
	{
		VValue Temp = *this; // Don't have printing path compress
		VPlaceholder& Placeholder = Temp.AsPlaceholder();
		Builder.Appendf(TEXT("Placeholder(0x%" PRIxPTR "->"), &Placeholder);
		VValue Pointee = Placeholder.Follow();
		if (Pointee.IsPlaceholder())
		{
			Builder.Appendf(TEXT("0x%" PRIxPTR), &Pointee.AsPlaceholder());
		}
		else
		{
			Pointee.ToString(Builder, Context, Formatter);
		}
		Builder.Append(TEXT(")"));
	}
	else if (IsUninitialized())
	{
		Builder.Append(TEXT("Uninitialized"));
	}
	else
	{
		V_DIE("Unhandled Verse value encoding: 0x%" PRIxPTR, GetEncodedBits());
	}
}

FString ToString(FAllocationContext Context, const FCellFormatter& CellFormatter, const VRestValue& Value)
{
	return Value.ToString(Context, CellFormatter);
}

void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& CellFormatter, const VRestValue& Value)
{
	return Value.ToString(Builder, Context, CellFormatter);
}

FString VRestValue::ToString(FAllocationContext Context, const FCellFormatter& Formatter) const
{
	return Value.Get().ToString(Context, Formatter);
}

void VRestValue::ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter) const
{
	return Value.Get().ToString(Builder, Context, Formatter);
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
