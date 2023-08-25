// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMOption.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VOption, VHeapValue, TEXT("Optional"));
TGlobalTrivialEmergentTypePtr<&VOption::StaticCppClassInfo> VOption::GlobalTrivialEmergentType;

void VOption::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VOption& This = ThisCell->StaticCast<VOption>();
	VHeapValue::MarkReferencedCellsImpl(&This, MarkStack);
	This.Value.Mark(MarkStack);
}

uint32 VOption::GetTypeHashImpl(VCell* ThisCell)
{
	VOption& ThisOption = ThisCell->StaticCast<VOption>();
	static constexpr uint32 MagicNumber = 0x9e3779b9;
	return ::HashCombineFast(static_cast<uint32>(MagicNumber), GetTypeHash(ThisOption.GetValue()));
}

} // namespace Verse
#endif // WITH_VERSE_VM