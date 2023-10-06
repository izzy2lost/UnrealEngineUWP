// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMSuspension.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMCaptureSwitch.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_VCPPCLASSINFO(VSuspension, VCell, TEXT("Suspension"));
DEFINE_VCPPCLASSINFO(VBytecodeSuspension, VSuspension, TEXT("BytecodeSuspension"));
DEFINE_VCPPCLASSINFO(VLambdaSuspension, VSuspension, TEXT("LambdaSuspension"));
TGlobalTrivialEmergentTypePtr<&VBytecodeSuspension::StaticCppClassInfo> VBytecodeSuspension::GlobalTrivialEmergentType;
TGlobalTrivialEmergentTypePtr<&VLambdaSuspension::StaticCppClassInfo> VLambdaSuspension::GlobalTrivialEmergentType;

void VSuspension::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VSuspension& This = ThisCell->StaticCast<VSuspension>();
	VCell::MarkReferencedCellsImpl(&This, MarkStack);
	This.FailureContext.Mark(MarkStack);
	This.Next.Mark(MarkStack);
}

void VBytecodeSuspension::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VBytecodeSuspension& This = ThisCell->StaticCast<VBytecodeSuspension>();
	VSuspension::MarkReferencedCellsImpl(&This, MarkStack);
	This.Procedure.Mark(MarkStack);
	This.CaptureSwitch([&MarkStack](auto& Captures) {
		Captures.ForEachOperand([&MarkStack](EOperandRole, auto Value) {
			Value.Mark(MarkStack); // Whether or not this is a `VValue` or `TWriteBarrier<T>`, just mark it.
		});
	});
}

void VLambdaSuspension::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VLambdaSuspension& This = ThisCell->StaticCast<VLambdaSuspension>();
	VSuspension::MarkReferencedCellsImpl(&This, MarkStack);
	for (size_t I = 0; I < This.NumValues; ++I)
	{
		This.Args()[I].Mark(MarkStack);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
