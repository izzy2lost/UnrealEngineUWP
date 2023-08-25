// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VEmergentType, VCell, TEXT("EmergentType"));

void VEmergentType::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VEmergentType* This = static_cast<VEmergentType*>(ThisCell);
	VCell::MarkReferencedCellsImpl(This, MarkStack);
	This->Type.Mark(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM