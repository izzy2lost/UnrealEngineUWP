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
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VTuple);

template <typename TVisitor>
void VTuple::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Values, NumValues);
}

bool VTuple::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VTuple>())
	{
		return false;
	}

	VTuple& OtherTuple = Other->StaticCast<VTuple>();
	if (Num() != OtherTuple.Num())
	{
		return false;
	}
	for (uint32 Index = 0, End = Num(); Index < End; ++Index)
	{
		if (!VValue::Equal(Context, GetValue(Index), OtherTuple.GetValue(Index), HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

uint32 VTuple::GetTypeHashImpl()
{
	const TWriteBarrier<VValue>* Ptr = Values;
	const uint32 Size = Num();
	return ::GetArrayHash(Ptr, Size);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
