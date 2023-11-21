// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMRestValue.h"

namespace Verse
{
void FAbstractVisitor::VisitNonNull(VCell* InCell, const char* ElementName)
{
}

void FAbstractVisitor::VisitNonNull(UObject* InObject, const char* ElementName)
{
}

void FAbstractVisitor::VisitAuxNonNull(void* InAux, const char* ElementName)
{
}

void FAbstractVisitor::BeginArray(const char* ElementName)
{
}

void FAbstractVisitor::EndArray()
{
}

void FAbstractVisitor::BeginSet(const char* ElementName)
{
}

void FAbstractVisitor::EndSet()
{
}

void FAbstractVisitor::BeginMap(const char* ElementName)
{
}

void FAbstractVisitor::EndMap()
{
}

void FAbstractVisitor::BeginObject()
{
}

void FAbstractVisitor::EndObject()
{
}

void FAbstractVisitor::VisitEmergentType(const VCell* InEmergentType)
{
	VisitNonNull(const_cast<VCell*>(InEmergentType), "EmergentType");
}

void FAbstractVisitor::Visit(VCell* InCell, const char* ElementName)
{
	if (InCell != nullptr)
	{
		VisitNonNull(InCell, ElementName);
	}
}

void FAbstractVisitor::Visit(UObject* InObject, const char* ElementName)
{
	if (InObject != nullptr)
	{
		VisitNonNull(InObject, ElementName);
	}
}

void FAbstractVisitor::VisitAux(void* InAux, const char* ElementName)
{
	if (InAux != nullptr)
	{
		VisitAuxNonNull(InAux, ElementName);
	}
}

void FAbstractVisitor::Visit(VValue Value, const char* ElementName)
{
	if (VCell* Cell = Value.ExtractCell())
	{
		Visit(Cell, ElementName);
	}
	else if (Value.IsUObject())
	{
		Visit(Value.AsUObject(), ElementName);
	}
}

void FAbstractVisitor::Visit(VRestValue& Value, const char* ElementName)
{
	Value.Visit(*this, ElementName);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
