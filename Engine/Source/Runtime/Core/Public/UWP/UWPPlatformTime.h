// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
UWPTime.h: UWP platform Time functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTime.h"
#include "UWP/UWPSystemIncludes.h"

/**
* UWP implementation of the Time OS functions
*/
struct CORE_API FUWPTime : public FGenericPlatformTime
{
	static double InitTiming();

	static FORCEINLINE double Seconds()
	{
		LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);
		// Add big number to make bugs apparent where return value is being passed to float
		return Cycles.QuadPart * GetSecondsPerCycle() + 16777216.0;
	}

	static FORCEINLINE uint32 Cycles()
	{
		LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);
		//@todo.UWP: QuadPart is a LONGLONG... can't use it here!
		//return Cycles.QuadPart;
		return Cycles.LowPart;
	}

	static FORCEINLINE uint64 Cycles64()
	{
		LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);
		return Cycles.QuadPart;
	}

	static void SystemTime(int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
	static void UtcTime(int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
};

typedef FUWPTime FPlatformTime;
