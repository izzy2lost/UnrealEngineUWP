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

	FORCEINLINE bool IsMarked(const VCell* InCell, const char* ElementName)
	{
		return FHeap::IsMarked(InCell);
	}

	// Structure notification methods with no implementation in the mark stack.  Will be optimized away
	FORCEINLINE void BeginArray(const char* ElementName)
	{
	}

	FORCEINLINE void EndArray()
	{
	}

	FORCEINLINE void BeginSet(const char* ElementName)
	{
	}

	FORCEINLINE void EndSet()
	{
	}

	FORCEINLINE void BeginMap(const char* ElementName)
	{
	}

	FORCEINLINE void EndMap()
	{
	}

	FORCEINLINE void BeginObject()
	{
	}

	FORCEINLINE void EndObject()
	{
	}

	void VisitNonNull(const VCell* InCell, const char* ElementName)
	{
		MarkStack.MarkNonNull(InCell);
	}

	void VisitNonNull(const UObject* InObject, const char* ElementName)
	{
		MarkStack.MarkNonNull(InObject);
	}

	void VisitAuxNonNull(const void* InAux, const char* ElementName)
	{
		MarkStack.MarkAuxNonNull(InAux);
	}

	FORCEINLINE void VisitEmergentType(const VCell* InEmergentType)
	{
		VisitNonNull(InEmergentType, "EmergentType");
	}

	FORCEINLINE void Visit(const VCell* InCell, const char* ElementName)
	{
		if (InCell != nullptr)
		{
			VisitNonNull(InCell, ElementName);
		}
	}

	FORCEINLINE void Visit(const UObject* InObject, const char* ElementName)
	{
		if (InObject != nullptr)
		{
			VisitNonNull(InObject, ElementName);
		}
	}

	FORCEINLINE void VisitAux(const void* Aux, const char* ElementName)
	{
		if (Aux != nullptr)
		{
			VisitAuxNonNull(Aux, ElementName);
		}
	}

	FORCEINLINE void Visit(VValue Value, const char* ElementName)
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

	FORCEINLINE void Visit(const VRestValue& Value, const char* ElementName)
	{
		Value.Visit(*this, ElementName);
	}

	// NOTE: The Value parameter can not be passed by value.
	template <typename T>
	FORCEINLINE void Visit(const TWriteBarrier<T>& Value, const char* ElementName)
	{
		Visit(Value.Get(), ElementName);
	}

	template <typename T>
	FORCEINLINE void Visit(T Begin, T End, const char* ElementName)
	{
		BeginArray(ElementName);
		for (; Begin != End; ++Begin)
		{
			Visit(*Begin, ElementName);
		}
		EndArray();
	}

	template <typename T>
	FORCEINLINE void Visit(T* Values, uint32 Count, const char* ElementName)
	{
		Visit(Values, Values + Count, ElementName);
	}

	template <typename T>
	FORCEINLINE void Visit(T* Values, uint64 Count, const char* ElementName)
	{
		Visit(Values, Values + Count, ElementName);
	}

	template <typename T>
	FORCEINLINE void Visit(const TArray<T>& Values, const char* ElementName)
	{
		Visit(Values.begin(), Values.end(), ElementName);
	}

	template <typename T>
	FORCEINLINE void Visit(const TSet<T>& Values, const char* ElementName)
	{
		BeginSet(ElementName);
		for (const auto& Value : Values)
		{
			Visit(Value, ElementName);
		}
		EndSet();
	}

	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
	FORCEINLINE void Visit(const TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Values, const char* ElementName)
	{
		BeginMap(ElementName);
		for (const auto& Kvp : Values)
		{
			BeginObject();
			Visit(Kvp.Key, "Key");
			Visit(Kvp.Value, "Value");
			EndObject();
		}
		EndMap();
	}

	void ReportNativeBytes(size_t Bytes)
	{
		MarkStack.ReportNativeBytes(Bytes);
	}

private:
	FMarkStack& MarkStack;
};

} // namespace Verse
