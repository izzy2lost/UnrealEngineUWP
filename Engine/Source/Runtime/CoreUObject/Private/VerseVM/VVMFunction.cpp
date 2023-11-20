// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFunction.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFunction);
TGlobalTrivialEmergentTypePtr<&VFunction::StaticCppClassInfo> VFunction::GlobalTrivialEmergentType;

template <typename TVisitor>
void VFunction::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Procedure, "Procedure");
	Visitor.Visit(ParentScope, "ParentScope");
}

void VFunction::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	Builder.Append(TEXT("Function(Procedure="));
	Formatter.Append(Builder, Context, *Procedure);
	Builder.Append(TEXT(")"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
