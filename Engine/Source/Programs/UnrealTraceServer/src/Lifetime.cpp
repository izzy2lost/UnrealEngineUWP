// Copyright Epic Games, Inc. All Rights Reserved.
#include "Pch.h"
#include "Lifetime.h"
#include "InstanceInfo.h"
#include "Logging.h"
#include "StoreService.h"

#if TS_USING(TS_PLATFORM_LINUX) || TS_USING(TS_PLATFORM_MAC)
#include <signal.h>
#endif

////////////////////////////////////////////////////////////////////////////////
FLifetime::FLifetime(class FStoreService* InStoreService)
	: StoreService(InStoreService)
{
	check(StoreService && InstanceInfo);
}

////////////////////////////////////////////////////////////////////////////////
bool FLifetime::ShouldKeepAlive()
{
	// Check if all sponsors are alive. If no sponsors are alive try to shutdown
	// the store if it has no active connections. 
	const bool bShouldKeepAlive = IsAnySponsorActive() || !ShutdownStoreIfNoConnections();
	return bShouldKeepAlive;
}

////////////////////////////////////////////////////////////////////////////////
void FLifetime::CheckNewSponsors(FInstanceInfo* InstanceInfo)
{
	check(InstanceInfo);
	for (auto& PidEntry : InstanceInfo->SponsorPids)
	{
		if (uint32_t ThisPid = PidEntry.load(std::memory_order_relaxed))
		{
			if (PidEntry.compare_exchange_strong(ThisPid, 0))
			{
				AddPid(ThisPid);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void FLifetime::AddPid(uint32 Pid)
{
	if (Pid == 0)
	{
		return;
	}
#if TS_USING(TS_PLATFORM_WINDOWS)
	FProcHandle ProcessHandle = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, Pid);
#else
	FProcHandle ProcessHandle = FProcHandle(intptr_t(Pid));
#endif
	SponsorHandles.FindOrAdd(ProcessHandle, [](FProcHandle& Handle, const FProcHandle& New) {return Handle == New ? 0 : 1; });
}

////////////////////////////////////////////////////////////////////////////////
bool FLifetime::ShutdownStoreIfNoConnections()
{
	return StoreService->ShutdownIfNoConnections();
}

////////////////////////////////////////////////////////////////////////////////
bool FLifetime::IsAnySponsorActive()
{
	bool bKeepAlive = false;
	for (auto& Handle : SponsorHandles)
	{
#if TS_USING(TS_PLATFORM_WINDOWS)
		DWORD ExitCode;
		const bool Result = GetExitCodeProcess(Handle, &ExitCode);
		if (Result && ExitCode == STILL_ACTIVE)
		{
			bKeepAlive = true;
		}
		else
		{
			Handle = 0;
		}
#else
		int Pid = int(intptr_t(Handle));
		if (kill(pid_t(Pid), 0) == 0)
		{
			bKeepAlive = true;
		}
		else
		{
			Handle = 0;
		}
#endif
	}

	// Remove inactive processes
	SponsorHandles.RemoveIf([](const FProcHandle& Handle) { return Handle == 0; });
	return bKeepAlive;
}

