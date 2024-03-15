// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMTask.h"
#include "VVMFailureContext.h"
#include "VVMFrame.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VTask);
TGlobalHeapPtr<VEmergentType> VTask::EmergentType;

template <typename TVisitor>
void VTask::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Result, TEXT("Result"));
	Visitor.Visit(Awaiters, TEXT("Awaiters"));
	Visitor.Visit(PrevAwait, TEXT("PrevAwait"));

	Visitor.Visit(YieldFrame, TEXT("YieldFrame"));
	Visitor.Visit(YieldTask, TEXT("YieldTask"));
	Visitor.Visit(FailureContext, TEXT("FailureContext"));

	Visitor.Visit(ResumeFrame, TEXT("ResumeFrame"));
	ResumeSlot.Visit(Visitor);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)