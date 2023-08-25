// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMFailureContext.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VFailureContext, VCell, TEXT("FailureContext"));
TGlobalTrivialEmergentTypePtr<&VFailureContext::StaticCppClassInfo> VFailureContext::GlobalTrivialEmergentType;

void VFailureContext::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VFailureContext& This = ThisCell->StaticCast<VFailureContext>();
	VCell::MarkReferencedCellsImpl(&This, MarkStack);

	This.FirstChild.Mark(MarkStack);
	This.Next.Mark(MarkStack);
	This.Prev.Mark(MarkStack);
	This.Parent.Mark(MarkStack);
	This.Frame.Mark(MarkStack);
	This.IncomingEffectToken.Mark(MarkStack);
	This.BeforeThenEffectToken.MarkReferencedCell(MarkStack);
	This.DoneEffectToken.MarkReferencedCell(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM