// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMRestValue.h"

namespace Verse
{

void VRestValue::MarkReferencedCell(FMarkStack& MarkStack)
{
	Value.Mark(MarkStack);
}

} // namespace Verse
#endif // WITH_VERSE_VM