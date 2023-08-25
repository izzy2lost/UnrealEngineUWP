// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMObject.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{
DEFINE_VCPPCLASSINFO(VObject, VHeapValue, TEXT("Object"));
} // namespace Verse
#endif // WITH_VERSE_VM