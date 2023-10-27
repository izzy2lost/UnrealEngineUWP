// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMType.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentType.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VType, VCell, TEXT("Type"));

VType::VType(FAllocationContext Context, EVerseTypeTag T)
	: VCell(Context, VEmergentTypeCreator::EmergentTypeForType.Get())
	, Tag(T)
{
}

TGlobalHeapPtr<VTrivialType> VTrivialType::Singleton;

void VTrivialType::Initialize(FAllocationContext Context)
{
	V_DIE_UNLESS(VEmergentTypeCreator::EmergentTypeForType);
	Singleton.Set(Context, new (Context.AllocateFastCell(sizeof(VTrivialType))) VTrivialType(Context));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)