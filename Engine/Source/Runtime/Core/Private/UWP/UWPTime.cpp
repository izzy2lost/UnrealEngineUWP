// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
UWPTime.cpp: UWP implementations of time functions
=============================================================================*/

//#include "CorePrivatePCH.h"
#include "UWPTime.h"

double FUWPTime::InitTiming()
{
	LARGE_INTEGER Frequency;
	verify(QueryPerformanceFrequency(&Frequency));
	SecondsPerCycle = 1.0 / Frequency.QuadPart;
	SecondsPerCycle64 = 1.0 / Frequency.QuadPart;
	return FPlatformTime::Seconds();
}

void FUWPTime::SystemTime(int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	SYSTEMTIME st;
	::GetLocalTime(&st);

	Year = st.wYear;
	Month = st.wMonth;
	DayOfWeek = st.wDayOfWeek;
	Day = st.wDay;
	Hour = st.wHour;
	Min = st.wMinute;
	Sec = st.wSecond;
	MSec = st.wMilliseconds;
}

void FUWPTime::UtcTime(int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	SYSTEMTIME st;
	GetSystemTime(&st);

	Year = st.wYear;
	Month = st.wMonth;
	DayOfWeek = st.wDayOfWeek;
	Day = st.wDay;
	Hour = st.wHour;
	Min = st.wMinute;
	Sec = st.wSecond;
	MSec = st.wMilliseconds;
}
