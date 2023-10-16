// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{

DEFINE_VISIT_REFERENCES(VPlaceholder);
DEFINE_VCPPCLASSINFO(VPlaceholder, VCell, TEXT("Placeholder"));
TGlobalTrivialEmergentTypePtr<&VPlaceholder::StaticCppClassInfo> VPlaceholder::GlobalTrivialEmergentType;

template <typename TVisitor>
void VPlaceholder::VisitReferencesImpl(TVisitor& Visitor)
{
	VCell::VisitReferences(this, Visitor);
	Visitor.Visit(Value);
}

} // namespace Verse
#endif // WITH_VERSE_VM
