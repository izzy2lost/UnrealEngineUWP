// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "HAL/PlatformMemory.h"
#include "GenericPlatform/GenericPlatformCriticalSection.h"

/**
 * This is the UWP version of a critical section. It uses an aggregate
 * CRITICAL_SECTION to implement its locking.
 */
class FUWPCriticalSection
{
	/**
	 * The UWP specific critical section
	 */
	CRITICAL_SECTION CriticalSection;

public:

	/**
	 * Constructor that initializes the aggregated critical section
	 */
	FORCEINLINE FUWPCriticalSection()
	{
#if USING_CODE_ANALYSIS
	MSVC_PRAGMA( warning( suppress : 28125 ) )	// warning C28125: The function 'InitializeCriticalSection' must be called from within a try/except block:  The requirement might be conditional.
#endif	// USING_CODE_ANALYSIS
		InitializeCriticalSectionEx(&CriticalSection, 1, 0);
	}

	/**
	 * Destructor cleaning up the critical section
	 */
	FORCEINLINE ~FUWPCriticalSection()
	{
		DeleteCriticalSection(&CriticalSection);
	}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock()
	{
		// Spin first before entering critical section, causing ring-0 transition and context switch.
		if( TryEnterCriticalSection(&CriticalSection) == 0 )
		{
			EnterCriticalSection(&CriticalSection);
		}
	}

	/**
	* Quick test for seeing if the lock is already being used.
	*/
	FORCEINLINE bool TryLock()
	{
		if (TryEnterCriticalSection(&CriticalSection))
		{
			return true;
		};
		return false;
	}

	/**
	 * Releases the lock on the critical seciton
	 */
	FORCEINLINE void Unlock()
	{
		LeaveCriticalSection(&CriticalSection);
	}
};

/**
 * FUwpRWLock - Read/Write Mutex
 *	- Provides non-recursive Read/Write (or shared-exclusive) access.
 *	- Windows specific lock structures/calls Ref: https://msdn.microsoft.com/en-us/library/windows/desktop/aa904937(v=vs.85).aspx
 */
class FUwpRWLock
{
public:
	FORCEINLINE FUwpRWLock(uint32 Level = 0)
	{
		InitializeSRWLock(&Mutex);
	}
	
	FORCEINLINE ~FUwpRWLock()
	{
	}
	
	FORCEINLINE void ReadLock()
	{
		AcquireSRWLockShared(&Mutex);
	}
	
	FORCEINLINE void WriteLock()
	{
		AcquireSRWLockExclusive(&Mutex);
	}
	
	FORCEINLINE void ReadUnlock()
	{
		ReleaseSRWLockShared(&Mutex);
	}
	
	FORCEINLINE void WriteUnlock()
	{
		ReleaseSRWLockExclusive(&Mutex);
	}
	
private:
	SRWLOCK Mutex;
};


typedef FUWPCriticalSection FCriticalSection;
typedef FSystemWideCriticalSectionNotImplemented FSystemWideCriticalSection;
typedef FUwpRWLock FRWLock;
