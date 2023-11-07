// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMValuePrinting.h"
#include "Containers/UnrealString.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMTupleInline.h"
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
#include "VerseVM/VVMTuple.h"
#include "VerseVM/VVMVar.h"
#include <inttypes.h>

namespace Verse
{
FString FDefaultCellFormatter::ToString(FAllocationContext Context, VCell& Cell) const
{
	if (Cell.IsA<VTuple>())
	{
		VTuple& Tuple = Cell.StaticCast<VTuple>();
		FString Result("Tuple(");
		for (uint32 Index = 0, End = Tuple.Num(); Index < End; ++Index)
		{
			if (Index > 0)
			{
				Result += ", ";
			}
			Result += Tuple.GetValue(Index).ToString(Context, *this);
		}
		Result += ")";
		return Result;
	}
	if (Cell.IsA<VVar>())
	{
		return FString::Printf(TEXT("Var(%s)"), *Cell.StaticCast<VVar>().Get(Context).ToString(Context, *this));
	}

	if (const ::Verse::VRational* Rational = Cell.DynamicCast<VRational>())
	{
		return FString::Printf(TEXT("Rational(%s / %s)"),
			*Rational->Numerator.Get().ToString(Context, *this),
			*Rational->Denominator.Get().ToString(Context, *this));
	}

	if (const ::Verse::VUniqueString* UniqueString = Cell.DynamicCast<VUniqueString>())
	{
		return FString::Printf(TEXT("UniqueString(\"%hs\"), address: %p"), UniqueString->AsCString(), &UniqueString);
	}
	else if (const ::Verse::VUTF8String* String = Cell.DynamicCast<VUTF8String>())
	{
		return FString::Printf(TEXT("String(\"%hs\")"), String->AsCString());
	}
	else if (const ::Verse::VUniqueStringSet* UniqueStringSet = Cell.DynamicCast<VUniqueStringSet>())
	{
		return Verse::ToString(Context, *UniqueStringSet);
	}
	else if (const ::Verse::VConstructor* Constructor = Cell.DynamicCast<VConstructor>())
	{
		return Verse::ToString(Context, *Constructor);
	}

	if (VValue Logic(Cell); Logic.IsLogic())
	{
		return Logic.AsBool() ? TEXT("true") : TEXT("false");
	}

	if (VFunction* Function = Cell.DynamicCast<VFunction>())
	{
		return FString::Printf(TEXT("Function(Procedure=%s)"), *ToString(Context, *Function->Procedure.Get()));
	}

	return FString::Printf(TEXT("Cell(0x%" PRIxPTR ")"), BitCast<uintptr_t>(&Cell));
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

FString ToString(const VHeapInt& HeapInt)
{
	FString NumberResult;

	if (HeapInt.IsZero())
	{
		NumberResult = "0";
	}
	else
	{
		for (int32 I = HeapInt.GetLength() - 1; I >= 0; --I)
		{
			NumberResult += FString::Printf(TEXT(" %08X"), HeapInt.GetDigit(I));
		}
	}

	return FString::Printf(TEXT("HeapInt(%hc%sh)"), HeapInt.GetSign() ? '-' : '+', *NumberResult);
}

FString ToString(double Double)
{
	return LexToString(Double);
}

FString ToString(FAllocationContext Context, const VValue& Value, const FCellFormatter& Formatter)
{
	return Value.ToString(Context, Formatter);
}

FString VValue::ToString(FAllocationContext Context, const FCellFormatter& Formatter) const
{
	if (*this == VValue::EffectDoneMarker())
	{
		return FString::Printf(TEXT("0x%x"), AsInt32());
	}
	else if (IsInt())
	{
		if (IsCellOfType<VHeapInt>())
		{
			// larger heap-ints
			return ::Verse::ToString(AsCell().StaticCast<VHeapInt>());
		}

		// Smaller ints
		return ::Verse::ToString(AsInt());
	}
	else if (IsFloat())
	{
		return ::Verse::ToString(AsFloat().AsDouble());
	}
	else if (IsCell())
	{
		return Formatter.ToString(Context, AsCell());
	}
	else if (IsRoot())
	{
		return FString::Printf(TEXT("Root(%u)"), GetSplitDepth());
	}
	else if (IsPlaceholder())
	{
		VValue Temp = *this; // Don't have printing path compress
		VPlaceholder& Placeholder = Temp.AsPlaceholder();
		FString Result = FString::Printf(TEXT("Placeholder(0x%" PRIxPTR "->"), &Placeholder);
		VValue Pointee = Placeholder.Follow();
		if (Pointee.IsPlaceholder())
		{
			Result += FString::Printf(TEXT("0x%" PRIxPTR), &Pointee.AsPlaceholder());
		}
		else
		{
			Result += Pointee.ToString(Context, Formatter);
		}
		Result += TEXT(")");
		return Result;
	}
	else if (IsUninitialized())
	{
		return TEXT("Uninitialized");
	}
	else
	{
		V_DIE("Unhandled Verse value encoding: 0x%" PRIxPTR, GetEncodedBits());
	}
}

FString ToString(FAllocationContext Context, const VRestValue& Value, const FCellFormatter& CellFormatter)
{
	return Value.ToString(Context, CellFormatter);
}

FString ToString(FAllocationContext Context, const VUniqueString& String)
{
	return FString::Printf(TEXT("UniqueString(\"%hs\"), address 0x%p"), String.AsCString(), &String);
}

FString ToString(FAllocationContext Context, const VUniqueStringSet& UniqueStringSet)
{
	FString Result = "UniqueStringSet( ";
	for (auto& CurrentString : UniqueStringSet)
	{
		Result += FString::Printf(TEXT("(\"%s\"), "), *ToString(Context, *CurrentString.Get()));
	}
	Result += FString::Printf(TEXT("), address 0x%p"), &UniqueStringSet);
	return Result;
}

FString ToString(FAllocationContext Context, const VConstructor& Constructor, const FCellFormatter& CellFormatter)
{
	FString Result = "Constructor(\n";
	for (uint32 Index = 0; Index < Constructor.NumEntries; ++Index)
	{
		const VConstructor::VEntry& Entry = Constructor.Entries[Index];
		const FString ConstantStringRepresentation = Entry.Value.Get().ToString(Context, CellFormatter);
		Result += FString::Printf(TEXT("\t%s : Entry(Value: %s, Dynamic: %s))\n"), *ToString(Context, *Entry.Name), *ConstantStringRepresentation, Entry.bDynamic ? "true" : "false");
	}
	Result += FString::Printf(TEXT(")"));
	return Result;
}

FString VRestValue::ToString(FAllocationContext Context, const FCellFormatter& CellFormatter) const
{
	return ::Verse::ToString(Context, Value.Get(), CellFormatter);
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
