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
	Visitor.Visit(FirstChild);
	Visitor.Visit(Next);
	Visitor.Visit(Prev);
	Visitor.Visit(Parent);
	Visitor.Visit(Frame);
	Visitor.Visit(IncomingEffectToken);
	Visitor.Visit(BeforeThenEffectToken);
	Visitor.Visit(DoneEffectToken);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)