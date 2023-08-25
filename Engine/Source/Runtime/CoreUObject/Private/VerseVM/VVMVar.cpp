// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMVar.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VVar, VCell, TEXT("Var"));
TGlobalTrivialEmergentTypePtr<&VVar::StaticCppClassInfo> VVar::GlobalTrivialEmergentType;

void VVar::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VVar* This = static_cast<VVar*>(ThisCell);
	VCell::MarkReferencedCellsImpl(This, MarkStack);
	This->Value.MarkReferencedCell(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM