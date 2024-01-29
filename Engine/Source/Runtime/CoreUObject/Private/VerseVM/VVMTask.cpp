// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMTask.h"
#include "VVMFailureContext.h"
#include "VVMFrame.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VTask);
TGlobalTrivialEmergentTypePtr<&VTask::StaticCppClassInfo> VTask::GlobalTrivialEmergentType;

template <typename TVisitor>
void VTask::VisitReferencesImpl(TVisitor& Visitor)
{
	TIntrusiveTree<VTask>::VisitReferencesImpl(Visitor);

	Visitor.Visit(YieldFrame, TEXT("YieldFrame"));
	Visitor.Visit(YieldTask, TEXT("YieldTask"));
	Visitor.Visit(FailureContext, TEXT("FailureContext"));

	Visitor.Visit(ResumeFrame, TEXT("ResumeFrame"));
	ResumeSlot.Visit(Visitor);
	Visitor.Visit(ResumeTask, TEXT("ResumeTask"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)