// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	UWPAtomics.h: UWP platform Atomics functions
==============================================================================================*/

#pragma once
#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "UWPSystemIncludes.h"
#include <intrin.h>

/**
 * UWP implementation of the Atomics OS functions
 */
struct CORE_API FUWPAtomics : public FGenericPlatformAtomics
{

	static FORCEINLINE int8 InterlockedIncrement(volatile int8* Value)
	{
		return (int8)_InterlockedExchangeAdd8((char*)Value, 1) + 1;
	}

	static FORCEINLINE int16 InterlockedIncrement(volatile int16* Value)
	{
		return (int16)_InterlockedIncrement16((short*)Value);
	}

	static FORCEINLINE int32 InterlockedIncrement(volatile int32* Value)
	{
		return (int32)_InterlockedIncrement((long*)Value);
	}

	static FORCEINLINE int64 InterlockedIncrement (volatile int64* Value)
	{
#if PLATFORM_64BITS
		return (int64)::_InterlockedIncrement64((int64*)Value);
#else
		// No explicit instruction for 64-bit atomic increment on 32-bit processors; has to be implemented in terms of CMPXCHG8B
		for (;;)
		{
			int64 OldValue = *Value;
			if (_InterlockedCompareExchange64(Value, OldValue + 1, OldValue) == OldValue)
			{
				return OldValue + 1;
			}
		}
#endif
	}

	static FORCEINLINE int8 InterlockedDecrement(volatile int8* Value)
	{
		return (int8)::_InterlockedExchangeAdd8((char*)Value, -1) - 1;
	}

	static FORCEINLINE int16 InterlockedDecrement(volatile int16* Value)
	{
		return (int16)::_InterlockedDecrement16((short*)Value);
	}

	static FORCEINLINE int32 InterlockedDecrement(volatile int32* Value)
	{
		return (int32)::_InterlockedDecrement((long*)Value);
	}

	static FORCEINLINE int64 InterlockedDecrement (volatile int64* Value)
	{
#if PLATFORM_64BITS
		return (int64)::_InterlockedDecrement64((int64*)Value);
#else
		// No explicit instruction for 64-bit atomic decrement on 32-bit processors; has to be implemented in terms of CMPXCHG8B
		for (;;)
		{
			int64 OldValue = *Value;
			if (_InterlockedCompareExchange64(Value, OldValue - 1, OldValue) == OldValue)
			{
				return OldValue - 1;
			}
		}
#endif
	}

	static FORCEINLINE int8 InterlockedAdd(volatile int8* Value, int8 Amount)
	{
		return (int8)::_InterlockedExchangeAdd8((char*)Value, (char)Amount);
	}

	static FORCEINLINE int16 InterlockedAdd(volatile int16* Value, int16 Amount)
	{
		return (int16)::_InterlockedExchangeAdd16((short*)Value, (short)Amount);
	}

	static FORCEINLINE int32 InterlockedAdd(volatile int32* Value, int32 Amount)
	{
		return (int32)::_InterlockedExchangeAdd((long*)Value, (long)Amount);
	}

	static FORCEINLINE int64 InterlockedAdd (volatile int64* Value, int64 Amount)
	{
#if PLATFORM_64BITS
		return (int64)::_InterlockedExchangeAdd64((int64*)Value, (int64)Amount);
#else
		// No explicit instruction for 64-bit atomic add on 32-bit processors; has to be implemented in terms of CMPXCHG8B
		for (;;)
		{
			int64 OldValue = *Value;
			if (_InterlockedCompareExchange64(Value, OldValue + Amount, OldValue) == OldValue)
			{
				return OldValue + Amount;
			}
		}
#endif
	}

	static FORCEINLINE int8 InterlockedExchange(volatile int8* Value, int8 Exchange)
	{
		return (int8)::_InterlockedExchange8((char*)Value, (char)Exchange);
	}

	static FORCEINLINE int16 InterlockedExchange(volatile int16* Value, int16 Exchange)
	{
		return (int16)::_InterlockedExchange16((short*)Value, (short)Exchange);
	}

	static FORCEINLINE int32 InterlockedExchange(volatile int32* Value, int32 Exchange)
	{
		return (int32)::_InterlockedExchange((long*)Value, (long)Exchange);
	}

	static FORCEINLINE int64 InterlockedExchange (volatile int64* Value, int64 Exchange)
	{
#if PLATFORM_64BITS
		return ::_InterlockedExchange64(Value, Exchange);
#else
		// No explicit instruction for 64-bit atomic exchange on 32-bit processors; has to be implemented in terms of CMPXCHG8B
		for (;;)
		{
			int64 OldValue = *Value;
			if (_InterlockedCompareExchange64(Value, Exchange, OldValue) == OldValue)
			{
				return OldValue;
			}
		}
#endif
	}

	static FORCEINLINE void* InterlockedExchangePtr( void** Dest, void* Exchange )
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (IsAligned(Dest) == false)
		{
			HandleAtomicsFailure(TEXT("InterlockedExchangePointer requires Dest pointer to be aligned to %d bytes"), sizeof(void*));
		}
#endif

		return ::_InterlockedExchangePointer(Dest, Exchange);
	}

	static FORCEINLINE int8 InterlockedCompareExchange(volatile int8* Dest, int8 Exchange, int8 Comparand)
	{
		return (int8)::_InterlockedCompareExchange8((char*)Dest, (char)Exchange, (char)Comparand);
	}

	static FORCEINLINE int16 InterlockedCompareExchange(volatile int16* Dest, int16 Exchange, int16 Comparand)
	{
		return (int16)::_InterlockedCompareExchange16((short*)Dest, (short)Exchange, (short)Comparand);
	}

	static FORCEINLINE int32 InterlockedCompareExchange(volatile int32* Dest,int32 Exchange,int32 Comparand)
	{
		return (int32)::_InterlockedCompareExchange((LPLONG)Dest,(LONG)Exchange,(LONG)Comparand);
	}

	static FORCEINLINE int64 InterlockedCompareExchange (volatile int64* Dest, int64 Exchange, int64 Comparand)
	{
		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (IsAligned(Dest, 8) == false)
			{
				HandleAtomicsFailure(TEXT("InterlockedCompareExchange64 requires args be aligned to %d bytes"), sizeof(int64));
			}
		#endif

		return (int64)::_InterlockedCompareExchange64((LONGLONG*)Dest, (LONGLONG)Exchange, (LONGLONG)Comparand);
	}

	static FORCEINLINE int8 AtomicRead(volatile const int8* Src)
	{
		return InterlockedCompareExchange((int8*)Src, 0, 0);
	}

	static FORCEINLINE int16 AtomicRead(volatile const int16* Src)
	{
		return InterlockedCompareExchange((int16*)Src, 0, 0);
	}

	static FORCEINLINE int32 AtomicRead(volatile const int32* Src)
	{
		return InterlockedCompareExchange((int32*)Src, 0, 0);
	}

	static FORCEINLINE int64 AtomicRead(volatile const int64* Src)
	{
		return InterlockedCompareExchange((int64*)Src, 0, 0);
	}

	static FORCEINLINE void AtomicStore(volatile int8* Src, int8 Val)
	{
		InterlockedExchange(Src, Val);
	}

	static FORCEINLINE void AtomicStore(volatile int16* Src, int16 Val)
	{
		InterlockedExchange(Src, Val);
	}

	static FORCEINLINE void AtomicStore(volatile int32* Src, int32 Val)
	{
		InterlockedExchange(Src, Val);
	}

	static FORCEINLINE void AtomicStore(volatile int64* Src, int64 Val)
	{
		InterlockedExchange(Src, Val);
	}

	/**
	 * Atomically compares the pointer to comparand and replaces with the exchange
	 * pointer if they are equal and returns the original value
	 */
	static FORCEINLINE void* InterlockedCompareExchangePointer(void** Dest,void* Exchange,void* Comparand)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Dest needs to be aligned otherwise the function will behave unpredictably 
		if (IsAligned( Dest ) == false)
		{
			HandleAtomicsFailure( TEXT( "InterlockedCompareExchangePointer requires Dest pointer to be aligned to %d bytes" ), sizeof(void*) );
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		return ::_InterlockedCompareExchangePointer(Dest,Exchange,Comparand);
	}

private:
	/** Handles atomics function failure. Since 'check' has not yet been declared here we need to call external function to use it. */
	static void HandleAtomicsFailure( const TCHAR* InFormat, ... );
};

typedef FUWPAtomics FPlatformAtomics;
