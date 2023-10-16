// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMVar.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{

DEFINE_VISIT_REFERENCES(VVar);
DEFINE_VCPPCLASSINFO(VVar, VCell, TEXT("Var"));
TGlobalTrivialEmergentTypePtr<&VVar::StaticCppClassInfo> VVar::GlobalTrivialEmergentType;

template <typename TVisitor>
void VVar::VisitReferencesImpl(TVisitor& Visitor)
{
	VCell::VisitReferences(this, Visitor);
	Visitor.Visit(Value);
}

} // namespace Verse
#endif // WITH_VERSE_VM
