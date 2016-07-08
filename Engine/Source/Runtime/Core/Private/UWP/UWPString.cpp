// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UWPString.cpp: UWP implementations of string functions
=============================================================================*/

#include "CorePrivatePCH.h"

int32 FUWPString::GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr )
{
#if USE_SECURE_CRT
	int32 Result = _vsntprintf_s(Dest,DestSize,Count/*_TRUNCATE*/,Fmt,ArgPtr);
#else
	int32 Result = _vsntprintf(Dest,Count,Fmt,ArgPtr);
#endif
	va_end( ArgPtr );
	return Result;
}

int32 FUWPString::GetVarArgs( ANSICHAR* Dest, SIZE_T DestSize, int32 Count, const ANSICHAR*& Fmt, va_list ArgPtr )
{
#if USE_SECURE_CRT
	int32 Result = _vsnprintf_s(Dest,DestSize,Count/*_TRUNCATE*/,Fmt,ArgPtr);
#else
	int32 Result = _vsnprintf(Dest,Count,Fmt,ArgPtr);
#endif
	va_end( ArgPtr );
	return Result;
}

