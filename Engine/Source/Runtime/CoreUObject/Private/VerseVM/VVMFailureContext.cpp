// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFailureContext.h"
#include "VVMFrame.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFailureContext);
TGlobalTrivialEmergentTypePtr<&VFailureContext::StaticCppClassInfo> VFailureContext::GlobalTrivialEmergentType;

template <typename TVisitor>
void VFailureContext::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(FirstChild, "FirstChild");
	Visitor.Visit(Next, "Next");
	Visitor.Visit(Prev, "Prev");
	Visitor.Visit(Parent, "Parent");
	Visitor.Visit(Frame, "Frame");
	Visitor.Visit(IncomingEffectToken, "IncomingEffectToken");
	Visitor.Visit(BeforeThenEffectToken, "BeforeThenEffectToken");
	Visitor.Visit(DoneEffectToken, "DoneEffectToken");
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)