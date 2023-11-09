// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMArray.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMTuple.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VArray);
TGlobalTrivialEmergentTypePtr<&VArray::StaticCppClassInfo> VArray::GlobalTrivialEmergentType;

template <typename TVisitor>
void VArray::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Tuple);
}

bool VArray::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (VArray* OtherArray = Other->DynamicCast<VArray>())
	{
		return GetTuple().Equal(Context, &OtherArray->GetTuple(), HandlePlaceholder);
	}
	return false;
}

uint32 VArray::GetTypeHashImpl()
{
	const TWriteBarrier<VValue>* Ptr = Tuple->Values;
	const uint32 Size = Tuple->Num();
	return ::GetArrayHash(Ptr, Size);
}

VArray::FConstIterator VArray::begin() const
{
	return Tuple.Get()->Values;
}

VArray::FConstIterator VArray::end() const
{
	return Tuple.Get()->Values + NumValues;
}

VArray::VArray(FAllocationContext Context, uint32 InitialCapacity)
	: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
	, NumValues(0)
{
	Tuple.Set(Context, &VTuple::New(Context, InitialCapacity));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
