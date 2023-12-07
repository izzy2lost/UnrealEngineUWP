// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMPackage.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VPackage);
TGlobalTrivialEmergentTypePtr<&VPackage::StaticCppClassInfo> VPackage::GlobalTrivialEmergentType;

template <typename TVisitor>
void VPackage::VisitReferencesImpl(TVisitor& Visitor)
{
	Map.Visit(Visitor, TEXT("DefinitionMap"));
	Visitor.Visit(DigestCode[(int)EDigestVariant::PublicAndEpicInternal], TEXT("PublicAndEpicInternalDigest"));
	Visitor.Visit(DigestCode[(int)EDigestVariant::PublicOnly], TEXT("PublicOnlyDigest"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
