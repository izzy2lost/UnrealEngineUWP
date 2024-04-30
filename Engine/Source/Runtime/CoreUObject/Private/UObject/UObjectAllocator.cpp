// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjAllocator.cpp: Unreal object allocation
=============================================================================*/

#include "UObject/UObjectAllocator.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectAllocator, Log, All);

/** Global UObjectBase allocator							*/
COREUOBJECT_API FUObjectAllocator GUObjectAllocator;

/**
 * Prints a debugf message to allow tuning
 */
void FUObjectAllocator::BootMessage()
{
	const SIZE_T ExceedingSize = GetPersistentLinearAllocator().GetExceedingSize();
	if (ExceedingSize > 0)
	{
		UE_LOG(LogUObjectAllocator, Warning, TEXT("Persistent memory pool exceeded by %u KB, please tune PersistentAllocatorReserveSizeMB setting in [MemoryPools] ini group."), ExceedingSize / 1024);
	}
}

/**
 * Allocates a UObjectBase from the free store or the permanent object pool
 *
 * @param Size size of uobject to allocate
 * @param Alignment alignment of uobject to allocate
 * @param bAllowPermanent if true, allow allocation in the permanent object pool, if it fits
 * @return newly allocated UObjectBase (not really a UObjectBase yet, no constructor like thing has been called).
 */
UObjectBase* FUObjectAllocator::AllocateUObject(int32 Size, int32 Alignment, bool bAllowPermanent)
{
	// we want to perform this allocation uninstrumented so the GC can clean this up if the transaction is aborted
	UE_AUTORTFM_OPEN(
	{
		if (bAllowPermanent)
		{
			// this allocation might go over the reserved memory amount and default to FMemory::Malloc, so we are moving it into the ARTFM scope
			return (UObjectBase*)GetPersistentLinearAllocator().Allocate(Size, Alignment);
		}

		return (UObjectBase*)FMemory::Malloc(Size, Alignment);
	});
}

/**
 * Returns a UObjectBase to the free store, unless it is in the permanent object pool
 *
 * @param Object object to free
 */
void FUObjectAllocator::FreeUObject(UObjectBase *Object) const
{
	check(Object);
	// Only free memory if it was allocated directly from allocator and not from permanent object pool.
	if (FPermanentObjectPoolExtents().Contains(Object) == false)
	{
		FMemory::Free(Object);
	}
	// We only destroy objects residing in permanent object pool during the exit purge.
	else
	{
		check(GExitPurge);
	}
}


