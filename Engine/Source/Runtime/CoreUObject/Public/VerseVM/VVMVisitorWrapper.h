// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "VVMRestValue.h"
#include "VVMValue.h"
#include "VVMWriteBarrier.h"

class UObject;

namespace Verse
{
struct VCell;

// Visitor wrapper is used to provide all the boilerplate functionality for either the abstract visitor or the
// mark stack visitor.
template <typename TVisitor>
struct TVisitorWrapper : TVisitor
{
	template <typename... Args>
	TVisitorWrapper(Args&&... InArgs)
		: TVisitor(std::forward<Args>(InArgs)...)
	{
	}

	FORCEINLINE void VisitNonNull(const VCell* InCell)
	{
		TVisitor::VisitNonNull(InCell);
	}

	FORCEINLINE void VisitNonNull(const UObject* InObject)
	{
		TVisitor::VisitNonNull(InObject);
	}
	FORCEINLINE void Visit(const VCell* InCell)
	{
		if (InCell != nullptr)
		{
			VisitNonNull(InCell);
		}
	}

	FORCEINLINE void Visit(const UObject* InObject)
	{
		if (InObject != nullptr)
		{
			VisitNonNull(InObject);
		}
	}

	FORCEINLINE void Visit(VValue Value)
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

	FORCEINLINE void Visit(const VRestValue& Value)
	{
		Value.Visit(*this);
	}

	template <typename T>
	FORCEINLINE void Visit(const TWriteBarrier<T>& Value)
	{
		Visit(Value.Get());
	}

	template <typename T>
	FORCEINLINE void Visit(T Begin, T End)
	{
		for (; Begin != End; ++Begin)
		{
			Visit(*Begin);
		}
	}

	template <typename T>
	FORCEINLINE void Visit(const T* Values, uint32 Count)
	{
		Visit(Values, Values + Count);
	}
};

} // namespace Verse
