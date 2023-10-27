// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMTuple.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMTupleInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMVisitorWrapper.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
DEFINE_VISIT_REFERENCES(VTuple)
DEFINE_VCPPCLASSINFO(VTuple, VHeapValue, TEXT("Tuple"));

template <typename TVisitor>
void VTuple::VisitReferencesImpl(TVisitor& Visitor)
{
	VHeapValue::VisitReferences(this, Visitor);
	Visitor.Visit(Values, NumValues);
}

bool VTuple::EqualImpl(FRunningContext Context, VCell* ThisCell, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder)
{
	if (!Other->IsA<VTuple>())
	{
		return false;
	}

	VTuple& ThisTuple = ThisCell->StaticCast<VTuple>();
	VTuple& OtherTuple = Other->StaticCast<VTuple>();
	if (ThisTuple.Num() != OtherTuple.Num())
	{
		return false;
	}
	for (uint32 Index = 0, End = ThisTuple.Num(); Index < End; ++Index)
	{
		if (!VValue::Equal(Context, ThisTuple.GetValue(Index), OtherTuple.GetValue(Index), HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

uint32 VTuple::GetTypeHashImpl(VCell* ThisCell)
{
	const VTuple& ThisTuple = ThisCell->StaticCast<VTuple>();
	const TWriteBarrier<VValue>* Ptr = ThisTuple.Values;
	const uint32 Size = ThisTuple.Num();
	return ::GetArrayHash(Ptr, Size);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
