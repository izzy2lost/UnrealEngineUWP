// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMSuspension.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMCaptureSwitch.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMFailureContext.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMProcedure.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VSuspension);
DEFINE_DERIVED_VCPPCLASSINFO(VBytecodeSuspension);
DEFINE_DERIVED_VCPPCLASSINFO(VLambdaSuspension);
TGlobalTrivialEmergentTypePtr<&VBytecodeSuspension::StaticCppClassInfo> VBytecodeSuspension::GlobalTrivialEmergentType;
TGlobalTrivialEmergentTypePtr<&VLambdaSuspension::StaticCppClassInfo> VLambdaSuspension::GlobalTrivialEmergentType;

template <typename TVisitor>
void VSuspension::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(FailureContext, "FailureContext");
	Visitor.Visit(Next, "Next");
}

template <typename TVisitor>
void VBytecodeSuspension::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Procedure, "Procedure");
	CaptureSwitch([&Visitor](auto& Captures) {
		Captures.ForEachOperand([&Visitor](EOperandRole, auto Value) {
			Visitor.Visit(Value, "Value"); // Whether or not this is a `VValue` or `TWriteBarrier<T>`, just mark it.
		});
	});
}

template <typename TVisitor>
void VLambdaSuspension::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Args(), NumValues, "Args");
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
