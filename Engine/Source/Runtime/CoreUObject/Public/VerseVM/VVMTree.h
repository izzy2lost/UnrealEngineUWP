// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
template <typename DerivedType>
struct TIntrusiveTree
{
	TWriteBarrier<DerivedType> Parent;
	TWriteBarrier<DerivedType> FirstChild;
	TWriteBarrier<DerivedType> Next;
	TWriteBarrier<DerivedType> Prev;

	TIntrusiveTree(FAccessContext Context, DerivedType* Parent)
		: Parent(Context, Parent)
	{
		DerivedType* This = static_cast<DerivedType*>(this);

		if (Parent)
		{
			if (Parent->FirstChild)
			{
				Parent->FirstChild->Prev.Set(Context, This);
				Next.Set(Context, Parent->FirstChild.Get());
			}
			Parent->FirstChild.Set(Context, This);
		}
	}

	void Detach(FAccessContext Context)
	{
		DerivedType* This = static_cast<DerivedType*>(this);

		if (Parent && Parent->FirstChild.Get() == This)
		{
			V_DIE_IF(Prev);
			Parent->FirstChild.Set(Context, Next.Get());
		}

		if (Next)
		{
			V_DIE_UNLESS(Next->Prev.Get() == This);
			Next->Prev.Set(Context, Prev.Get());
		}
		if (Prev)
		{
			V_DIE_UNLESS(Prev->Next.Get() == This);
			Prev->Next.Set(Context, Next.Get());
		}

		Next.Set(Context, nullptr);
		Prev.Set(Context, nullptr);
	}

	// Visit each element of the subtree rooted at `this`.
	template <typename FunctionType>
	void ForEach(FunctionType&& Function)
	{
		if (LIKELY(!FirstChild.Get()))
		{
			Function(static_cast<DerivedType&>(*this));
			return;
		}

		TArray<TIntrusiveTree*> ToVisit;
		ToVisit.Push(this);
		while (ToVisit.Num())
		{
			TIntrusiveTree* Task = ToVisit.Pop();
			Function(static_cast<DerivedType&>(*Task));
			for (TIntrusiveTree* Child = Task->FirstChild.Get(); Child; Child = Child->Next.Get())
			{
				ToVisit.Push(Child);
			}
		}
	}

	template <typename TVisitor>
	void VisitReferencesImpl(TVisitor& Visitor)
	{
		Visitor.Visit(Parent, TEXT("Parent"));
		Visitor.Visit(FirstChild, TEXT("FirstChild"));
		Visitor.Visit(Next, TEXT("Next"));
		Visitor.Visit(Prev, TEXT("Prev"));
	}
};
} // namespace Verse
