// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/CastedTo.h"

enum class ESmartPointer
{
	Strong,
	Shared = Strong, // Shared and Strong pointers do the same thing, which is prevent automatic memory deletion using different systems (Garbage Collection vs. Reference Counting)
	Weak,
	// Unique(?) - not sure if needed
};

/* Smart Pointer Class where the pointer can be statically casted to 'T' */
template<typename T>
class TSmartPtr
{
public:
	TSmartPtr() : Container(MakeShared<TPointerContainer<T>>()) {}

	template<typename From>
	TSmartPtr(From* InObject)
	{
		Set(InObject, ESmartPointer::Weak);
	}

	template<typename From>
	TSmartPtr(From* InObject, const ESmartPointer InType)
	{
		Set(InObject, InType);
	}

	template<typename From>
	TSmartPtr<T>& Set(From* InObject, const ESmartPointer InType)
	{
		if (InType == ESmartPointer::Strong || InType == ESmartPointer::Shared)
		{
			Container = MakeShared<TStrongCastable<From, T>>(InObject);
		}
		else if (InType == ESmartPointer::Weak)
		{
			Container = MakeShared<TWeakCastable<From, T>>(InObject);
		}

		return *this;
	}

	T* Get() const { return Container->Get(); }
	bool IsValid() const { return Container->IsValid(); }
	void Reset() const { Container->Reset(); }

private:
	TSharedRef<TPointerContainer<T>> Container = MakeShared<TPointerContainer<T>>();
};