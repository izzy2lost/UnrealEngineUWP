// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMFrame.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VFrame, VCell, TEXT("Frame"));
TGlobalTrivialEmergentTypePtr<&VFrame::StaticCppClassInfo> VFrame::GlobalTrivialEmergentType;

void VFrame::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VFrame& This = ThisCell->StaticCast<VFrame>();
	VCell::MarkReferencedCellsImpl(&This, MarkStack);
	This.ReturnEffectToken.MarkReferencedCell(MarkStack);
	This.Procedure.Mark(MarkStack);
	This.ReturnSlot.Mark(MarkStack);
	This.CallerFrame.Mark(MarkStack);
	for (uint32 Index = This.NumRegisters; Index--;)
	{
		This.Registers[Index].MarkReferencedCell(MarkStack);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
