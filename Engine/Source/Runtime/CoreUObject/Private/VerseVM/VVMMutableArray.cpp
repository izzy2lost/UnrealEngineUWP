// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VMutableArray);
DEFINE_TRIVIAL_VISIT_REFERENCES(VMutableArray);
TGlobalTrivialEmergentTypePtr<&VMutableArray::StaticCppClassInfo> VMutableArray::GlobalTrivialEmergentType;

void VMutableArray::SerializeImpl(VMutableArray*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint64 ScratchNumValues = 0;
		Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
		This = &VMutableArray::New(Context, (uint32)ScratchNumValues);
		for (uint32 Index = (uint32)ScratchNumValues; Index != 0; --Index)
		{
			VValue ScratchValue;
			Visitor.Visit(ScratchValue, TEXT(""));
			This->AddValue(Context, ScratchValue);
		}
		Visitor.EndArray();
	}
	else
	{
		uint64 ScratchNumValues = This->Num();
		Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
		Visitor.Visit(This->GetData(), This->GetData() + This->Num());
		Visitor.EndArray();
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
