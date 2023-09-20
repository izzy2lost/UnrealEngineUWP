// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VFunction, VCell, TEXT("Function"));
TGlobalTrivialEmergentTypePtr<&VFunction::StaticCppClassInfo> VFunction::GlobalTrivialEmergentType;

void VFunction::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VFunction& This = ThisCell->StaticCast<VFunction>();
	VCell::MarkReferencedCellsImpl(&This, MarkStack);
	This.Procedure.Mark(MarkStack);
	for (uint32 Index = This.NumCaptures; Index--;)
	{
		This.Captures[Index].Mark(MarkStack);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
