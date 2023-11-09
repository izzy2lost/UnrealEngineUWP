// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMCell.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include <type_traits>

namespace Verse
{
DEFINE_BASE_VCPPCLASSINFO(VCell);
DEFINE_DERIVED_VCPPCLASSINFO(VHeapValue);

static_assert(std::is_trivially_destructible_v<VCell>);

VCell::VCell(FAccessContext Context, const VEmergentType* EmergentType)
	: EmergentTypeOffset(FHeap::EmergentTypePtrToOffset(EmergentType))
{
	checkSlow(FHeap::OwnsAddress(this));
	// TODO: assert that if the emergent type has a destructor, that this cell is allocated in an appropriate subspace
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
	VCppClassInfo* CppClassInfo = GetEmergentType()->CppClassInfo;
	checkSlow(CppClassInfo->RunDestructor);
	CppClassInfo->RunDestructor(this);
}

bool VCell::Equal(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	return GetEmergentType()->CppClassInfo->Equal(Context, this, Other, HandlePlaceholder);
}

void VCell::ConductCensusImpl()
{
}

bool VCell::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	V_DIE("VCell subtype without `EqualImpl` override called! Either this type should have an override "
		  "if comparable OR a non-comparable type is being compared which is an error.");
	return false;
}

uint32 VCell::GetTypeHashImpl()
{
	V_DIE("VCell subtype without `GetTypeHashImpl` override called! Either this type should have an override "
		  "if hashable OR a non-hashable type is being hashed which is an error.");
	return 0;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
