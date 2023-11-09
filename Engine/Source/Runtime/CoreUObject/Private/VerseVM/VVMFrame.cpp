// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFrame.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMProcedure.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFrame);
TGlobalTrivialEmergentTypePtr<&VFrame::StaticCppClassInfo> VFrame::GlobalTrivialEmergentType;

template <typename TVisitor>
void VFrame::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(ReturnEffectToken);
	Visitor.Visit(Procedure);
	Visitor.Visit(ReturnSlot);
	Visitor.Visit(CallerFrame);
	Visitor.Visit(Registers, NumRegisters);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
