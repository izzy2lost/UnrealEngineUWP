// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMRestValue.h"

namespace Verse
{
void FAbstractVisitor::VisitNonNull(VCell* InCell)
{
}

void FAbstractVisitor::VisitNonNull(UObject* InObject)
{
}

void FAbstractVisitor::VisitEmergentType(const VCell* InEmergentType)
{
	VisitNonNull(const_cast<VCell*>(InEmergentType));
}

void FAbstractVisitor::Visit(VCell* InCell)
{
	if (InCell != nullptr)
	{
		VisitNonNull(InCell);
	}
}

void FAbstractVisitor::Visit(UObject* InObject)
{
	if (InObject != nullptr)
	{
		VisitNonNull(InObject);
	}
}

void FAbstractVisitor::Visit(VValue Value)
{
	if (VCell* Cell = Value.ExtractCell())
	{
		Visit(Cell);
	}
	else if (Value.IsUObject())
	{
		Visit(Value.AsUObject());
	}
}

void FAbstractVisitor::Visit(VRestValue& Value)
{
	Value.Visit(*this);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
