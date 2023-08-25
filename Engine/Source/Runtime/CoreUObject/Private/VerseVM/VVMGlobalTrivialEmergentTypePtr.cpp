// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"

namespace Verse
{

void FGlobalTrivialEmergentTypePtrRoot::MarkReferencedCells(FMarkStack& MarkStack)
{
	EmergentType.Mark(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM