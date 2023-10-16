// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMOption.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{

DEFINE_VISIT_REFERENCES(VOption);
DEFINE_VCPPCLASSINFO(VOption, VHeapValue, TEXT("Optional"));
TGlobalTrivialEmergentTypePtr<&VOption::StaticCppClassInfo> VOption::GlobalTrivialEmergentType;

template <typename TVisitor>
void VOption::VisitReferencesImpl(TVisitor& Visitor)
{
	VHeapValue::VisitReferences(this, Visitor);
	Visitor.Visit(Value);
}

uint32 VOption::GetTypeHashImpl(VCell* ThisCell)
{
	VOption& ThisOption = ThisCell->StaticCast<VOption>();
	static constexpr uint32 MagicNumber = 0x9e3779b9;
	return ::HashCombineFast(static_cast<uint32>(MagicNumber), GetTypeHash(ThisOption.GetValue()));
}

} // namespace Verse
#endif // WITH_VERSE_VM