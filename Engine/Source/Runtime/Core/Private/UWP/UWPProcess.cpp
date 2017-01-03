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
			DWORD ResultCode = GetModuleFileName(NULL, Result, _countof(Result));
			verify(ResultCode != 0);
			DWORD ErrorCode = ::GetLastError();
			verify(ErrorCode != ERROR_INSUFFICIENT_BUFFER);

			// Strip out the filename and add the trailing '\'
			FString BaseDir;
			BaseDir = FPaths::GetPath(FString(Result));
			BaseDir += TEXT("\\");
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
	FString PackageRelativePath(Filename);
	FPaths::MakePathRelativeTo(PackageRelativePath, *(FPaths::RootDir() + TEXT("/")));
	return ::LoadPackagedLibrary(*PackageRelativePath, 0ul);
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

// @ATG_CHANGE : BEGIN thread affinity addition
// This is a drop in equivelent to the deprecated thread affinity APIs that most legacy code is based on.
// As with those older APIs, it's functionality may be unexpected on machines with more than 64 cores.
DWORD_PTR WINAPI SetThreadAffinityMask(
	_In_ HANDLE    hThread,
	_In_ DWORD_PTR dwThreadAffinityMask
)
{
	static struct CPU_INFO {
		int Count;
		ULONG CpuInfoBytes;
		uint8_t* CpuInfoBuffer;
		PSYSTEM_CPU_SET_INFORMATION* CpuInfoPtrs;

		CPU_INFO() {
			Count = 0;
			CpuInfoBytes = 0;
			GetSystemCpuSetInformation(nullptr, 0, &CpuInfoBytes, GetCurrentProcess(), 0);

			CpuInfoBuffer = reinterpret_cast<uint8_t*>(malloc(CpuInfoBytes));
			if (CpuInfoBuffer)
			{
				GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(CpuInfoBuffer),
					CpuInfoBytes, &CpuInfoBytes, GetCurrentProcess(), 0);

				BYTE* firstCpu = CpuInfoBuffer;
				for (BYTE* currentCpu = firstCpu; currentCpu < firstCpu + CpuInfoBytes; currentCpu += reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(currentCpu)->Size)
				{
					Count++;
				}

				CpuInfoPtrs = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION*>(calloc(Count, sizeof(PSYSTEM_CPU_SET_INFORMATION)));

				if (CpuInfoPtrs)
				{
					firstCpu = CpuInfoBuffer;
					int currentCpuNum = 0;
					for (BYTE* currentCpu = firstCpu; currentCpu < firstCpu + CpuInfoBytes; currentCpu += reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(currentCpu)->Size)
					{
						CpuInfoPtrs[currentCpuNum] = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(currentCpu);
						currentCpuNum++;
					}
				}
			}
		}
	} Cpu_Info;

	// if static initialization got Cpu info....
	if (Cpu_Info.Count > 0 && Cpu_Info.CpuInfoBytes && Cpu_Info.CpuInfoBuffer && Cpu_Info.CpuInfoPtrs)
	{
		ULONGLONG priorMask = 0xffffffffffffffffull;
		ULONG priorCoreCount = 0;
		ULONG coreIds[64];

		// this is simplified, assuming that not other setter will be setting affinity
		if (GetThreadSelectedCpuSets(hThread, coreIds, 64, &priorCoreCount))
		{
			if (priorCoreCount > 0 && priorCoreCount <= 64)
			{
				priorMask = 0;
				for (unsigned int currentCoreEntry = 0; currentCoreEntry < priorCoreCount; currentCoreEntry++)
				{
					for (int i = 0; i < Cpu_Info.Count; i++)
					{
						if (coreIds[currentCoreEntry] == Cpu_Info.CpuInfoPtrs[i]->CpuSet.Id)
						{
							priorMask |= (1ull << i);
							break;
						}
					}
				}
			}
		}

		int markedCount = 0;
		for (int coreNum = 0; coreNum < 64 && coreNum < Cpu_Info.Count; coreNum++)
		{
			if (dwThreadAffinityMask & (1ull << coreNum))
			{
				coreIds[markedCount] = Cpu_Info.CpuInfoPtrs[coreNum]->CpuSet.Id;
				markedCount++;
			}
		}

		if (SetThreadSelectedCpuSets(hThread, coreIds, markedCount))
		{
			return priorMask;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return E_FAIL;
	}
}

void FUWPProcess::SetThreadAffinityMask(uint64 AffinityMask)
{
	::SetThreadAffinityMask(::GetCurrentThread(), (DWORD_PTR)AffinityMask);
}
// @ATG_CHANGE : END thread affinity addition

PACK_WINRT_REVERT()

#include "HideWindowsPlatformTypes.h"
// @ATG_CHANGE : END
