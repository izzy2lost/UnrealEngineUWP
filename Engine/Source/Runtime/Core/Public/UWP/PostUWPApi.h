// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostUWPApi.h: The post-include part of UE4's UWP API wrapper.
=============================================================================*/

// Undo any UWP defines.
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef CDECL
#undef PF_MAX
#undef PlaySound
#undef DrawText
#undef CaptureStackBackTrace
#undef MemoryBarrier
#undef DeleteFile
#undef MoveFile
#undef CopyFile
#undef CreateDirectory

// Redefine CDECL to our version of the #define.
#define CDECL	    __cdecl					/* Standard C function */
// Redefine CDECL to our version of the #define.
#define CDECL	    __cdecl					/* Standard C function */

#undef InterlockedIncrement
#undef InterlockedDecrement
#undef InterlockedAdd
#undef InterlockedExchange
#undef InterlockedExchangeAdd
#undef InterlockedCompareExchange
#undef InterlockedCompareExchangePointer
#undef InterlockedExchange64
#undef InterlockedExchangeAdd64
#undef InterlockedCompareExchange64
#undef InterlockedIncrement64
#undef InterlockedDecrement64
