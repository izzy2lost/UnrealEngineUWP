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
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		Visitor.Visit(CallerFrame, TEXT("CallerFrame"));
		ReturnSlot.Visit(Visitor);
		Visitor.Visit(Procedure, TEXT("Procedure"));
		uint64 ScratchNumRegisters = NumRegisters;
		Visitor.BeginArray(TEXT("Registers"), ScratchNumRegisters);
		Visitor.Visit(Registers, Registers + NumRegisters);
		Visitor.EndArray();
	}
	else
	{
		Visitor.Visit(CallerFrame, TEXT("CallerFrame"));
		ReturnSlot.Visit(Visitor);
		Visitor.Visit(Procedure, TEXT("Procedure"));
		Visitor.Visit(Registers, Registers + NumRegisters);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
