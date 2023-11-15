// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "VVMMarkStack.h"
#include "VVMRestValue.h"

namespace Verse
{

struct FMarkStackVisitor
{
	UE_NONCOPYABLE(FMarkStackVisitor);

	FMarkStackVisitor(FMarkStack& InMarkStack)
		: MarkStack(InMarkStack)
	{
	}

	bool IsMarked(const void* Ptr)
	{
		return FHeap::IsMarked(Ptr);
	}

	void VisitNonNull(const VCell* InCell)
	{
		MarkStack.MarkNonNull(InCell);
	}

	void VisitNonNull(const UObject* InObject)
	{
		MarkStack.MarkNonNull(InObject);
	}

	FORCEINLINE void VisitEmergentType(const VCell* InEmergentType)
	{
		VisitNonNull(InEmergentType);
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

	// NOTE: The Value parameter can not be passed by value.
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
	FORCEINLINE void Visit(T* Values, uint32 Count)
	{
		Visit(Values, Values + Count);
	}

	void ReportNativeBytes(size_t Bytes)
	{
		MarkStack.ReportNativeBytes(Bytes);
	}

private:
	FMarkStack& MarkStack;
};

} // namespace Verse
