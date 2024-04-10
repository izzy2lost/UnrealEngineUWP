// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMMutableArrayInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMOpResult.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VMutableArray);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMutableArray);
TGlobalTrivialEmergentTypePtr<&VMutableArray::StaticCppClassInfo> VMutableArray::GlobalTrivialEmergentType;

void VMutableArray::Append(FAllocationContext Context, VArrayBase& Array)
{
	if (!GetData())
	{
		Capacity = Array.Num();
		AllocateBuffer(Context, Array.GetArrayType(), Capacity);
	}
	else if (GetArrayType() != EArrayType::VValue && GetArrayType() != Array.GetArrayType())
	{
		Capacity = Num() + Array.Num();
		ConvertDataToVValues(Context, &Capacity);
	}

	switch (GetArrayType())
	{
		case EArrayType::None:
			// Empty-Untyped VMutableArray appending Empty-Untyped VMutableArray
			break;
		case EArrayType::VValue:
			Append<TWriteBarrier<VValue>>(Context, Array);
			break;
		case EArrayType::Int32:
			Append<int32>(Context, Array);
			break;
		case EArrayType::Char8:
			Append<uint8>(Context, Array);
			break;
		case EArrayType::Char32:
			Append<uint32>(Context, Array);
			break;
		default:
			V_DIE("Unhandled EArrayType encountered!");
	}
}

VValue VMutableArray::FreezeImpl(FRunningContext Context)
{
	EArrayType ArrayType = GetArrayType();
	VArray& FrozenArray = VArray::New(Context, Num(), ArrayType);
	if (ArrayType != EArrayType::VValue)
	{
		FMemory::Memcpy(FrozenArray.GetData(), GetData(), ByteLength());
	}
	else
	{
		for (uint32 I = 0; I < Num(); ++I)
		{
			FrozenArray.SetValue(Context, I, VValue::Freeze(Context, GetValue(I)));
		}
	}
	return FrozenArray;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
