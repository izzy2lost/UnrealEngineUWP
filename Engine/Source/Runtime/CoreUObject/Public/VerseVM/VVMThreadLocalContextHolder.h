// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMLog.h"

namespace Verse
{
struct FContextImpl;

// For use by FContextImpl.
struct FThreadLocalContextHolder
{
	FThreadLocalContextHolder() = default;
	COREUOBJECT_API ~FThreadLocalContextHolder();

	void Set(FContextImpl* InContext)
	{
		V_DIE_IF(Context);
		V_DIE_UNLESS(InContext);
		Context = InContext;
	}

	FContextImpl* Get() const
	{
		return Context;
	}

private:
	FContextImpl* Context = nullptr;
};

} // namespace Verse
