// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMPackage.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{

DEFINE_VISIT_REFERENCES(VPackage);
DEFINE_VCPPCLASSINFO(VPackage, VHeapValue, TEXT("Package"));
TGlobalTrivialEmergentTypePtr<&VPackage::StaticCppClassInfo> VPackage::GlobalTrivialEmergentType;

template <typename TVisitor>
void VPackage::VisitReferencesImpl(TVisitor& Visitor)
{
	VHeapValue::VisitReferences(this, Visitor);
	Visitor.Visit(NameAndDefinitions);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
