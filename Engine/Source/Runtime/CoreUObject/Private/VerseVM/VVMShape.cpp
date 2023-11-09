// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMShape.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VShape);
TGlobalTrivialEmergentTypePtr<&VShape::StaticCppClassInfo> VShape::GlobalTrivialEmergentType;

VShape::VShape(FAllocationContext Context, FieldsMap&& InFields)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, Fields(MoveTemp(InFields))
{
	// Re-order the offset-based fields on construction here. We do this in `VShape`'s constructor since this
	// is the point where it actually matters; when the offsets are being used to look up into an object's data.
	uint64 CurrentIndex = 0;
	for (auto& Pair : Fields)
	{
		switch (Pair.Value.Type)
		{
			case EFieldType::Offset:
				Pair.Value.Index = CurrentIndex++;
				break;
			case EFieldType::Constant:
			default:
				break;
		}
	}
	NumIndexedFields = CurrentIndex;
}

template <typename TVisitor>
void VShape::VisitReferencesImpl(TVisitor& Visitor)
{
	for (auto It = Fields.CreateIterator(); It; ++It)
	{
		switch (It->Value.Type)
		{
			case EFieldType::Offset:
				break;
			case EFieldType::Constant:
				Visitor.Visit(It->Value.Value);
				break;
		}
		Visitor.Visit(It->Key);
	}
}

VShape* VShape::New(FAllocationContext Context, FieldsMap&& InFields)
{
	// We allocate in the destructor space here since we're making `VShape` destructible so that it can
	// destruct its `TMap` member of fields.
	return new (Context.Allocate(FHeap::DestructorSpace, sizeof(VShape))) VShape(Context, MoveTemp(InFields));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
