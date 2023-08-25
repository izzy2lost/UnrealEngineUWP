// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMShape.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{
VShape::VEntry::VEntry(const VEntry& Other)
	: Type(Other.Type)
{
	switch (Type)
	{
		case EFieldType::Offset:
			Index = Other.Index;
			break;
		case EFieldType::Constant:
			new (&Constant) TWriteBarrier<VValue>(Other.Constant);
			break;
		default:
			break;
	}
};

VShape::VEntry::VEntry(VEntry&& Other)
	: Type(Other.Type)
{
	switch (Type)
	{
		case EFieldType::Offset:
			Move(Index, Other.Index);
			break;
		case EFieldType::Constant:
			new (&Constant) TWriteBarrier<VValue>(MoveTemp(Other.Constant));
			break;
		default:
			break;
	}
};

VShape::VEntry::VEntry(const uint64 InIndex)
	: Index(InIndex)
	, Type(EFieldType::Offset){};

VShape::VEntry::VEntry(FAccessContext Context, VValue InConstant)
	: Constant(TWriteBarrier<VValue>{Context, InConstant})
	, Type(EFieldType::Constant){};

DEFINE_VCPPCLASSINFO(VShape, VHeapValue, TEXT("Shape"));
TGlobalTrivialEmergentTypePtr<&VShape::StaticCppClassInfo> VShape::GlobalTrivialEmergentType;

VShape::VShape(FAllocationContext Context, VShape::FieldsMap&& InFields)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, Fields(MoveTemp(InFields))
{
}

void VShape::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VHeapValue::MarkReferencedCellsImpl(ThisCell, MarkStack);
	VShape* This = static_cast<VShape*>(ThisCell);
	for (auto It = This->Fields.CreateIterator(); It; ++It)
	{
		switch (It->Value.Type)
		{
			case EFieldType::Constant:
				It->Value.Constant.Mark(MarkStack);
				break;
			case EFieldType::Offset:
				break;
			default:
				VERSE_UNREACHABLE();
		}
		// Also want to mark the string as still being used.
		It->Key.Mark(MarkStack);
	}
}

VShape* VShape::New(FAllocationContext Context, VShape::FieldsMap&& InFields)
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