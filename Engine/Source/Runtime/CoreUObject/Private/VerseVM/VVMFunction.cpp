// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFunction.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFunction);
TGlobalTrivialEmergentTypePtr<&VFunction::StaticCppClassInfo> VFunction::GlobalTrivialEmergentType;

template <typename TVisitor>
void VFunction::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Procedure);
	Visitor.Visit(ParentScope);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
