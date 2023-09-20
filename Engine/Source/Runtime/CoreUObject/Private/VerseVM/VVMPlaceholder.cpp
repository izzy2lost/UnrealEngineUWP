// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VPlaceholder, VCell, TEXT("Placeholder"));
TGlobalTrivialEmergentTypePtr<&VPlaceholder::StaticCppClassInfo> VPlaceholder::GlobalTrivialEmergentType;

void VPlaceholder::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VPlaceholder& This = ThisCell->StaticCast<VPlaceholder>();
	VCell::MarkReferencedCellsImpl(&This, MarkStack);
	This.Value.Mark(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM
