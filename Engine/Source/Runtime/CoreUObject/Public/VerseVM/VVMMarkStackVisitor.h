// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "VVMMarkStack.h"
#include "VVMVisitorWrapper.h"

namespace Verse
{

struct FMarkStackVisitorDispatch
{
	UE_NONCOPYABLE(FMarkStackVisitorDispatch);

	FMarkStackVisitorDispatch(FMarkStack& InMarkStack)
		: MarkStack(InMarkStack)
	{
	}

	void VisitNonNull(const VCell* InCell)
	{
		MarkStack.MarkNonNull(InCell);
	}

	void VisitNonNull(const UObject* InObject)
	{
		MarkStack.MarkNonNull(InObject);
	}

private:
	FMarkStack& MarkStack;
};

using FMarkStackVisitor = TVisitorWrapper<FMarkStackVisitorDispatch>;

} // namespace Verse
