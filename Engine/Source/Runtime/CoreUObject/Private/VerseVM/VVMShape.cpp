// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMShape.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VFields, VHeapValue, TEXT("Fields"));
TGlobalTrivialEmergentTypePtr<&VFields::StaticCppClassInfo> VFields::GlobalTrivialEmergentType;

DEFINE_VCPPCLASSINFO(VShape, VHeapValue, TEXT("Shape"));
TGlobalTrivialEmergentTypePtr<&VShape::StaticCppClassInfo> VShape::GlobalTrivialEmergentType;

VShape::VShape(FAllocationContext Context, VFields::FieldsMap&& InFields)
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
			case EFieldType::Mutable:
			case EFieldType::Offset:
				Pair.Value.Index = CurrentIndex++;
				break;
			case EFieldType::Constant:
			default:
				break;
		}
	}
}

VShape* VShape::New(FAllocationContext Context, VFields::FieldsMap&& InFields)
{
	// We allocate in the destructor space here since we're making `VShape` destructible so that it can
	// destruct its `TMap` member of fields.
	return new (Context.Allocate(FHeap::DestructorSpace, sizeof(VShape))) VShape(Context, MoveTemp(InFields));
}

void VShape::RunDestructorImpl(VCell* This)
{
	VShape& ThisShape = This->StaticCast<VShape>();
	ThisShape.~VShape();
}

} // namespace Verse
#endif // WITH_VERSE_VM
