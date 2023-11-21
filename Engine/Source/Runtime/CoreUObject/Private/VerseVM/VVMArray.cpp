// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMArray.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VArray);
TGlobalTrivialEmergentTypePtr<&VArray::StaticCppClassInfo> VArray::GlobalTrivialEmergentType;

template <typename TVisitor>
inline void VArray::VisitReferencesImpl(TVisitor& Visitor)
{
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
