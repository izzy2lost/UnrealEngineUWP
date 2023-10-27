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
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{

DEFINE_VISIT_REFERENCES(VSuspension);
DEFINE_VISIT_REFERENCES(VBytecodeSuspension);
DEFINE_VISIT_REFERENCES(VLambdaSuspension);
DEFINE_VCPPCLASSINFO(VSuspension, VCell, TEXT("Suspension"));
DEFINE_VCPPCLASSINFO(VBytecodeSuspension, VSuspension, TEXT("BytecodeSuspension"));
DEFINE_VCPPCLASSINFO(VLambdaSuspension, VSuspension, TEXT("LambdaSuspension"));
TGlobalTrivialEmergentTypePtr<&VBytecodeSuspension::StaticCppClassInfo> VBytecodeSuspension::GlobalTrivialEmergentType;
TGlobalTrivialEmergentTypePtr<&VLambdaSuspension::StaticCppClassInfo> VLambdaSuspension::GlobalTrivialEmergentType;

template <typename TVisitor>
void VSuspension::VisitReferencesImpl(TVisitor& Visitor)
{
	VCell::VisitReferences(this, Visitor);
	Visitor.Visit(FailureContext);
	Visitor.Visit(Next);
}

template <typename TVisitor>
void VBytecodeSuspension::VisitReferencesImpl(TVisitor& Visitor)
{
	VSuspension::VisitReferences(this, Visitor);
	Visitor.Visit(Procedure);
	CaptureSwitch([&Visitor](auto& Captures) {
		Captures.ForEachOperand([&Visitor](EOperandRole, auto Value) {
			Visitor.Visit(Value); // Whether or not this is a `VValue` or `TWriteBarrier<T>`, just mark it.
		});
	});
}

template <typename TVisitor>
void VLambdaSuspension::VisitReferencesImpl(TVisitor& Visitor)
{
	VSuspension::VisitReferences(this, Visitor);
	Visitor.Visit(Args(), NumValues);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
