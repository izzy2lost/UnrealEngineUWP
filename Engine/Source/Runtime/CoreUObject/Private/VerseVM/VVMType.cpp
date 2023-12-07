// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMType.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentType.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VType);
DEFINE_TRIVIAL_VISIT_REFERENCES(VType);

VType::VType(FAllocationContext Context, EVerseTypeTag T)
	: VCell(Context, VEmergentTypeCreator::EmergentTypeForType.Get())
	, Tag(T)
{
}

DEFINE_DERIVED_VCPPCLASSINFO(VTrivialType)
DEFINE_TRIVIAL_VISIT_REFERENCES(VTrivialType);

TGlobalHeapPtr<VTrivialType> VTrivialType::Singleton;

void VTrivialType::Initialize(FAllocationContext Context)
{
	V_DIE_UNLESS(VEmergentTypeCreator::EmergentTypeForType);
	Singleton.Set(Context, new (Context.AllocateFastCell(sizeof(VTrivialType))) VTrivialType(Context));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)