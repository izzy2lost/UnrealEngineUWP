// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	UWPProcess.h: UWP platform Process functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformProcess.h"

/** Dummy process handle for platforms that use generic implementation. */
struct FProcHandle : public TProcHandle<void*, nullptr>
{
public:
	/** Default constructor. */
	FORCEINLINE FProcHandle()
		: TProcHandle()
	{}

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle( HandleType Other )
		: TProcHandle( Other )
	{}
};

/**
 * UWP implementation of the Process OS functions
 */
struct CORE_API FUWPProcess : public FGenericPlatformProcess
{
	static const TCHAR* BaseDir();
	static void Sleep( float Seconds );
	static void SleepNoStats(float Seconds); 
	static void SleepInfinite();
	static class FEvent* CreateSynchEvent(bool bIsManualReset = false);
	static class FRunnableThread* CreateRunnableThread();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static void* GetDllHandle(const TCHAR* Filename);
	static void FreeDllHandle(void* DllHandle);
	// @ATG_CHANGE : BEGIN UWP packaging & F5 support
	static void SetCurrentWorkingDirectoryToBaseDir();
	// @ATG_CHANGE : END

	// @ATG_CHANGE : BEGIN thread affinity addition
	static void SetThreadAffinityMask(uint64 AffinityMask);
	// @ATG_CHANGE : END 
	static const TCHAR* UserDir();
	static const TCHAR* UserSettingsDir();
	static const TCHAR* UserTempDir();
	static const TCHAR* ApplicationSettingsDir();
};

typedef FUWPProcess FPlatformProcess;

#include "../UWP/UWPCriticalSection.h"
typedef FUWPCriticalSection FCriticalSection;
typedef FSystemWideCriticalSectionNotImplemented FSystemWideCriticalSection;


