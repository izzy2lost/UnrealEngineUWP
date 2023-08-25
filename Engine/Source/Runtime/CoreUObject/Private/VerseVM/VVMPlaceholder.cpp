// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VPlaceholder, VCell, TEXT("Placeholder"));
TGlobalTrivialEmergentTypePtr<&VPlaceholder::StaticCppClassInfo> VPlaceholder::GlobalTrivialEmergentType;

void VPlaceholder::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VPlaceholder* This = static_cast<VPlaceholder*>(ThisCell);
	VCell::MarkReferencedCellsImpl(This, MarkStack);
	This->Value.Mark(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM