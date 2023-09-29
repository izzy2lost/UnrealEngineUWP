// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMEnumerator.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VEnumerator, VCell, TEXT("Enumerator"));
TGlobalTrivialEmergentTypePtr<&VEnumerator::StaticCppClassInfo> VEnumerator::GlobalTrivialEmergentType;

} // namespace Verse
#endif // WITH_VERSE_VM