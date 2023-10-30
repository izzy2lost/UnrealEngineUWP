// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VNativeFunction);
DEFINE_TRIVIAL_VISIT_REFERENCES(VNativeFunction);
TGlobalTrivialEmergentTypePtr<&VNativeFunction::StaticCppClassInfo> VNativeFunction::GlobalTrivialEmergentType;

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)