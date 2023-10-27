// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMCell.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{
DEFINE_VISIT_REFERENCES(VCell);
DEFINE_VCPPCLASSINFO_IMPL(VCell, nullptr, TEXT("Cell"));
DEFINE_VCPPCLASSINFO(VHeapValue, VCell, TEXT("HeapValue"));

VCell::VCell(FAccessContext Context, const VEmergentType* EmergentType)
	: EmergentTypeOffset(FHeap::EmergentTypePtrToOffset(EmergentType))
{
	checkSlow(FHeap::OwnsAddress(this));
	Context.RunWriteBarrierNonNull(EmergentType);
}

void VCell::SetEmergentType(FAccessContext Context, VEmergentType* EmergentType)
{
	Context.RunWriteBarrierNonNull(EmergentType);
	EmergentTypeOffset = FHeap::EmergentTypePtrToOffset(EmergentType);
}

FString VCell::DebugName() const
{
	return GetEmergentType()->CppClassInfo->DebugName();
}

void VCell::ConductCensus()
{
	GetEmergentType()->CppClassInfo->ConductCensus(this);
}

void VCell::RunDestructor()
{
	GetEmergentType()->CppClassInfo->RunDestructor(this);
}

bool VCell::Equal(FRunningContext Context, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder)
{
	return GetEmergentType()->CppClassInfo->Equal(Context, this, Other, HandlePlaceholder);
}

template <typename TVisitor>
void VCell::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.VisitNonNull(GetEmergentType());
}

void VCell::ConductCensusImpl(VCell* This)
{
}

void VCell::RunDestructorImpl(VCell* This)
{
}

bool VCell::EqualImpl(FRunningContext Context, VCell* This, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder)
{
	V_DIE("VCell subtype without `EqualImpl` override called! Either this type should have an override "
		  "if comparable OR a non-comparable type is being compared which is an error.");
	return false;
}

uint32 VCell::GetTypeHashImpl(VCell* This)
{
	V_DIE("VCell subtype without `GetTypeHashImpl` override called! Either this type should have an override "
		  "if hashable OR a non-hashable type is being hashed which is an error.");
	return 0;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
