// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
UWPProcess.cpp: UWP implementations of Process functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "UWPRunnableThread.h"

#include "AllowWindowsPlatformTypes.h"

PACK_WINRT()

const TCHAR* FUWPProcess::BaseDir()
{
	static bool bFirstTime = true;
	static TCHAR Result[512]=TEXT("");
	if (!Result[0] && bFirstTime)
	{
		// Check the commandline for -BASEDIR=<base directory>
		const TCHAR* CmdLine = NULL;
		if (FCommandLine::IsInitialized())
		{
			CmdLine = FCommandLine::Get();
		}
		// @ATG_CHANGE : code path that was hitting BaseDir in static initializers was removed
		//               and command line parsing was moved to earlier in launch to help avoid having 
		//               to have command line parsing code in multiple location.
		//               There's still potential for error here until Epic allows platform-subclassing the
		//               command line with an overridable Initialize() or BeginInitialize().
		bool overridden = false;
		if (CmdLine != NULL)
		{
			FString BaseDirToken = TEXT("-BASEDIR=");
			FString NextToken;
			while (FParse::Token(CmdLine, NextToken, false))
			{
				if (NextToken.StartsWith(BaseDirToken) == true)
				{
					FString BaseDir = NextToken.Right(NextToken.Len() - BaseDirToken.Len());
					BaseDir.ReplaceInline(TEXT("\\"), TEXT("/"));
					FCString::Strcpy(Result, *BaseDir);
					overridden = true;
				}
			}
		}

		if (!overridden)
		{
			Platform::String^ LocationPath = Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LocationPath = %s\n"), LocationPath->Data());
			FString BaseDir = LocationPath->Data();
			BaseDir = BaseDir / FApp::GetGameName() / TEXT("Binaries");
			BaseDir = BaseDir / FPlatformProperties::PlatformName();
// @ATG_CHANGE : BEGIN UWP packaging & F5 support
			BaseDir += PLATFORM_64BITS ? TEXT("64") : TEXT("32");
			BaseDir += "/";
			BaseDir.ReplaceInline(TEXT("/"), TEXT("\\"));
// @ATG_CHANGE : END
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("BaseDir = %s\n"), *BaseDir);
			FCString::Strcpy(Result, *BaseDir);
		}
		bFirstTime = false;
	}
	return Result;
}

#include "HideWindowsPlatformTypes.h"

void FUWPProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_Sleep);
	FThreadIdleStats::FScopeIdle Scope;
	::Sleep((int32)(Seconds * 1000.0));
}

void FUWPProcess::SleepNoStats(float Seconds)
{
	::Sleep((uint32)(Seconds * 1000.0));
}

void FUWPProcess::SleepInfinite()
{
	::Sleep(INFINITE);
}

#include "UWPEvent.h"

FEvent* FUWPProcess::CreateSynchEvent(bool bIsManualReset /*= false*/)
{
	// Allocate the new object
	FEvent* Event = NULL;
	if (FPlatformProcess::SupportsMultithreading())
	{
		Event = new FEventUWP();
	}
	else
	{
		// Fake vent object.
		Event = new FSingleThreadEvent();
	}
	// If the internal create fails, delete the instance and return NULL
	if (!Event->Create(bIsManualReset))
	{
		delete Event;
		Event = NULL;
	}
	return Event;
}

#include "AllowWindowsPlatformTypes.h"

bool FEventUWP::Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats /*= false*/)
{
	SCOPE_CYCLE_COUNTER(STAT_EventWait);
	FThreadIdleStats::FScopeIdle Scope(bIgnoreThreadIdleStats);
	check(Event);

	//		return (WaitForSingleObject(Event, WaitTime) == WAIT_OBJECT_0);
	return WaitForSingleObjectEx(Event, WaitTime, FALSE) == WAIT_OBJECT_0;
}

FRunnableThread* FUWPProcess::CreateRunnableThread()
{
	return new FRunnableThreadUWP();
}

const TCHAR* FUWPProcess::ExecutableName(bool bRemoveExtension)
{
	static TCHAR Result[512] = TEXT("");
	static TCHAR ResultWithExt[512] = TEXT("");
	if (!Result[0])
	{
		// Get complete path for the executable
		if (GetModuleFileName(NULL, Result, ARRAY_COUNT(Result)) != 0)
		{
			// Remove all of the path information by finding the base filename
			FString FileName = Result;
			FString FileNameWithExt = Result;
			FCString::Strncpy(Result, *(FPaths::GetBaseFilename(FileName)), ARRAY_COUNT(Result));
			FCString::Strncpy(ResultWithExt, *(FPaths::GetCleanFilename(FileNameWithExt)), ARRAY_COUNT(ResultWithExt));
		}
		// If the call failed, zero out the memory to be safe
		else
		{
			FMemory::Memzero(Result, sizeof(Result));
			FMemory::Memzero(ResultWithExt, sizeof(ResultWithExt));
		}
	}

	return (bRemoveExtension ? Result : ResultWithExt);
}

void* FUWPProcess::GetDllHandle(const TCHAR* Filename)
{
	check(Filename);
	return ::LoadPackagedLibrary(Filename, 0ul);
}

void FUWPProcess::FreeDllHandle(void* DllHandle)
{
	// It is okay to call FreeLibrary on 0
	::FreeLibrary((HMODULE)DllHandle);
}

// @ATG_CHANGE : BEGIN UWP packaging & F5 support
void FUWPProcess::SetCurrentWorkingDirectoryToBaseDir()
{
	FPlatformMisc::CacheLaunchDir();
	verify(SetCurrentDirectoryW(BaseDir()));
}

const TCHAR* FUWPProcess::UserDir()
{
	static TCHAR Result[PLATFORM_MAX_FILEPATH_LENGTH] = TEXT("");

	if (!Result[0])
	{
		Windows::Storage::StorageFolder ^ LocalFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
		if (nullptr != LocalFolder)
		{
			Platform::String^ LocalPath = LocalFolder->Path;
			if (nullptr != LocalPath)
			{
				FCString::Strncpy(Result, LocalPath->Data(), PLATFORM_MAX_FILEPATH_LENGTH);
			}
		}

		check(Result[0]);
	}

	return Result;
}

const TCHAR* FUWPProcess::UserSettingsDir()
{
	// @todo is there any reason to make this a sub-folder?
	return UserDir();
}

const TCHAR* FUWPProcess::UserTempDir()
{
	static TCHAR Result[PLATFORM_MAX_FILEPATH_LENGTH] = TEXT("");

	if (!Result[0])
	{
		Windows::Storage::StorageFolder ^ TempFolder = Windows::Storage::ApplicationData::Current->TemporaryFolder;
		if (nullptr != TempFolder)
		{
			Platform::String^ TempPath = TempFolder->Path;
			if (nullptr != TempPath)
			{
				FCString::Strncpy(Result, TempPath->Data(), PLATFORM_MAX_FILEPATH_LENGTH);
			}
		}

		check(Result[0]);
	}

	return Result;
}

const TCHAR* FUWPProcess::ApplicationSettingsDir()
{
	// This is supposed to be a writeable location that exists across multiple users.
	// For UWP it will have to be user-specific.
	return UserSettingsDir();
}

PACK_WINRT_REVERT()

#include "HideWindowsPlatformTypes.h"
// @ATG_CHANGE : END
