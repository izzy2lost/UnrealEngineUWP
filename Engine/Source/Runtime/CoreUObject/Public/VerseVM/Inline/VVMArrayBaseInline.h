// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMArrayBase.h"
#include "VerseVM/VVMAtomics.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMMutableArray.h"

namespace Verse
{

inline bool VArrayBase::IsInBounds(uint32 Index) const
{
	return Index < Num();
}

inline bool VArrayBase::IsInBounds(const VInt& Index, const uint32 Bounds) const
{
	if (Index.IsInt64())
	{
		const int64 IndexInt64 = Index.AsInt64();
		return (IndexInt64 >= 0) && (IndexInt64 < Bounds);
	}
	else
	{
		// Array maximum size is limited to the maximum size of a unsigned 32-bit integer.
		// So even if it's a `VHeapInt`, if it fails the `IsInt64` check, it is definitely out-of-bounds.
		return false;
	}
}

inline VValue VArrayBase::GetValue(uint32 Index)
{
	checkSlow(IsInBounds(Index));
	switch (GetArrayType())
	{
		case EArrayType::VValue:
			return BitCast<TAux<TWriteBarrier<VValue>>>(Values.Get())[Index].Follow();
		case EArrayType::Int32:
			return VValue::FromInt32(BitCast<TAux<int32>>(Values.Get())[Index]);
		case EArrayType::Char8:
			return VValue::Char(BitCast<TAux<uint8>>(Values.Get())[Index]);
		case EArrayType::Char32:
			return VValue::Char32(BitCast<TAux<uint32>>(Values.Get())[Index]);
		default:
			V_DIE("Unhandled EArrayType encountered!");
	}
}

inline void VArrayBase::ConvertDataToVValues(FAllocationContext Context, const uint32* Capacity)
{
	V_DIE_IF(IsA<VMutableArray>() && !Capacity);
	if (GetArrayType() != EArrayType::VValue)
	{
		const uint32 NewCapacity = Capacity ? *Capacity : Num();

		TAux<TWriteBarrier<VValue>> NewValues(Context.AllocateAuxCell(sizeof(TWriteBarrier<VValue>) * NewCapacity));
		for (uint32 Index = 0; Index < Num(); ++Index)
		{
			new (&NewValues[Index]) TWriteBarrier<VValue>(Context, GetValue(Index));
		}

		storeStoreFence();
		Values.Set(Context, BitCast<TAux<void>>(NewValues));
		SetArrayType(EArrayType::VValue);
	}
}

inline void VArrayBase::SetValue(FAllocationContext Context, uint32 Index, VValue Value, const uint32* Capacity)
{
	checkSlow(IsInBounds(Index));
	EArrayType ArrayType = GetArrayType();
	if (ArrayType == EArrayType::VValue)
	{
		SetVValue(Context, Index, Value);
	}
	else if (ArrayType != DetermineArrayType(Value))
	{
		ConvertDataToVValues(Context, Capacity);
		SetVValue(Context, Index, Value);
	}
	else
	{
		switch (ArrayType)
		{
			case EArrayType::Int32:
				SetInt32(Index, Value.AsInt32());
				break;
			case EArrayType::Char8:
				SetChar(Index, Value.AsChar());
				break;
			case EArrayType::Char32:
				SetChar32(Index, Value.AsChar32());
				break;
			default:
				V_DIE("Unhandled EArrayType encountered!");
		}
	}
}

template <typename T>
void VArrayBase::Serialize(T*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint8 ScratchArrayType;
		Visitor.Visit(ScratchArrayType, TEXT("ArrayType"));
		EArrayType ArrayType = static_cast<EArrayType>(ScratchArrayType);

		uint64 ScratchNumValues = 0;
		if (ArrayType != EArrayType::VValue)
		{
			Visitor.Visit(ScratchNumValues, TEXT("NumValues"));
			This = &T::New(Context, (uint32)ScratchNumValues, ArrayType);
			Visitor.VisitBulkData(This->GetData(), This->ByteLength(), TEXT("Values"));
			This->NumValues = ScratchNumValues; // Need to do this for VMutableArrays
		}
		else
		{
			Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
			This = &T::New(Context, (uint32)ScratchNumValues, ArrayType);
			Visitor.Visit(This->template GetData<TWriteBarrier<VValue>>(), This->template GetData<TWriteBarrier<VValue>>() + ScratchNumValues);
			Visitor.EndArray();
			This->NumValues = ScratchNumValues; // Need to do this for VMutableArrays
		}
	}
	else
	{
		EArrayType ArrayType = This->GetArrayType();
		uint8 ScratchArrayType = static_cast<uint8>(ArrayType);
		Visitor.Visit(ScratchArrayType, TEXT("ArrayType"));

		uint64 ScratchNumValues = This->Num();
		if (ArrayType != EArrayType::VValue)
		{
			Visitor.Visit(ScratchNumValues, TEXT("NumValues"));
			Visitor.VisitBulkData(This->GetData(), This->ByteLength(), TEXT("Values"));
		}
		else
		{
			Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
			Visitor.Visit(This->template GetData<TWriteBarrier<VValue>>(), This->template GetData<TWriteBarrier<VValue>>() + ScratchNumValues);
			Visitor.EndArray();
		}
	}
}

template <typename TVisitor>
inline void VArrayBase::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.VisitAux(GetData(), TEXT("ValuesBuffer")); // Visit the buffer we allocated for the array as Aux memory

	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumValues = Num();
		Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
		switch (GetArrayType())
		{
			case EArrayType::None:
				// Empty-Untyped VMutableArray
				break;
			case EArrayType::VValue:
				Visitor.Visit(GetData<TWriteBarrier<VValue>>(), GetData<TWriteBarrier<VValue>>() + Num());
				break;
			case EArrayType::Int32:
				Visitor.Visit(GetData<int32>(), GetData<int32>() + Num());
				break;
			case EArrayType::Char8:
				Visitor.Visit(GetData<uint8>(), GetData<uint8>() + Num());
				break;
			case EArrayType::Char32:
				Visitor.Visit(GetData<uint32>(), GetData<uint32>() + Num());
				break;
			default:
				V_DIE("Unhandled EArrayType encountered!");
		}
		Visitor.EndArray();
	}
	else if (GetArrayType() == EArrayType::VValue) // Check if we contain elements requiring marking
	{
		Visitor.Visit(GetData<TWriteBarrier<VValue>>(), GetData<TWriteBarrier<VValue>>() + Num()); // Visit allocated elements in the buffer
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
