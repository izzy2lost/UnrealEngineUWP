// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{
DEFINE_VISIT_REFERENCES(VEmergentType)
DEFINE_VCPPCLASSINFO(VEmergentType, VCell, TEXT("EmergentType"));

template <typename TVisitor>
void VEmergentType::VisitReferencesImpl(TVisitor& Visitor)
{
	VCell::VisitReferences(this, Visitor);
	Visitor.Visit(Shape);
	Visitor.Visit(Type);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
