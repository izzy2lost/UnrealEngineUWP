// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMArray.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMTuple.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
DEFINE_VCPPCLASSINFO(VArray, VHeapValue, TEXT("Array"));
TGlobalTrivialEmergentTypePtr<&VArray::StaticCppClassInfo> VArray::GlobalTrivialEmergentType;

void VArray::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VArray* This = static_cast<VArray*>(ThisCell);
	VHeapValue::MarkReferencedCellsImpl(This, MarkStack);
	This->Tuple.Mark(MarkStack);
}

bool VArray::EqualImpl(FRunningContext Context, VCell* ThisCell, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder)
{
	if (!Other->IsA<VArray>())
	{
		return false;
	}
	ThisCell = &ThisCell->StaticCast<VArray>().GetTuple();
	Other = &Other->StaticCast<VArray>().GetTuple();
	return VTuple::EqualImpl(Context, ThisCell, Other, HandlePlaceholder);
}

uint32 VArray::GetTypeHashImpl(VCell* ThisCell)
{
	const VTuple& Tuple = ThisCell->StaticCast<VArray>().GetTuple();
	const TWriteBarrier<VValue>* Ptr = Tuple.Values;
	const uint32 Size = Tuple.Num();
	return ::GetArrayHash(Ptr, Size);
}

VArray::VArray(FAllocationContext Context, uint32 Capacity)
	: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
	, NumValues(0)
{
	Tuple.Set(Context, &VTuple::New(Context, Capacity));
}

} // namespace Verse
#endif // WITH_VERSE_VM