// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VNativeFunction, VHeapValue, TEXT("NativeFunction"));
TGlobalTrivialEmergentTypePtr<&VNativeFunction::StaticCppClassInfo> VNativeFunction::GlobalTrivialEmergentType;

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)