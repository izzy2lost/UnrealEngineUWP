// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMutableMap.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VMutableMap);
TGlobalTrivialEmergentTypePtr<&VMutableMap::StaticCppClassInfo> VMutableMap::GlobalTrivialEmergentType;

template <typename TVisitor>
inline void VMutableMap::VisitReferencesImpl(TVisitor& Visitor)
{
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
