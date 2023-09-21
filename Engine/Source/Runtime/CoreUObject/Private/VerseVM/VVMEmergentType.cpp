// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VEmergentType, VCell, TEXT("EmergentType"));

void VEmergentType::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VEmergentType& This = ThisCell->StaticCast<VEmergentType>();
	VCell::MarkReferencedCellsImpl(&This, MarkStack);
	This.Shape.Mark(MarkStack);
	This.Type.Mark(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM
