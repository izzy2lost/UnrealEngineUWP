// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VPackage, VHeapValue, TEXT("Package"));
TGlobalTrivialEmergentTypePtr<&VPackage::StaticCppClassInfo> VPackage::GlobalTrivialEmergentType;

void VPackage::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VPackage* This = static_cast<VPackage*>(ThisCell);
	VHeapValue::MarkReferencedCellsImpl(This, MarkStack);
	This->NameAndDefinitions.Mark(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM