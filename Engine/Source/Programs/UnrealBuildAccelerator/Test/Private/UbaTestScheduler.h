// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaTestSession.h"
#include "UbaScheduler.h"

namespace uba
{
	bool TestLocalSchedule(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{

				Scheduler scheduler(session, true);

				ProcessStartInfo processInfo;
				processInfo.application = GetPingApplication();
				processInfo.workingDir = workingDir;
				processInfo.arguments = IsWindows ? TC("-n 2 localhost") : TC("-c 2 localhost");

				scheduler.EnqueueProcess(processInfo);
				scheduler.Start();

				u32 queued, active, finished;
				do { scheduler.GetStats(queued, active, finished); } while (queued != 0 || active != 0);

				scheduler.Stop();
				return true;
			});
	}

	bool TestLocalScheduleReuse(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		if (!IsWindows)
			return true;

		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{

				Scheduler scheduler(session, ~0u, true);

				StringBuffer<> testApp;
				GetTestAppPath(logger, testApp);

				ProcessStartInfo processInfo;
				processInfo.application = testApp.data;
				processInfo.workingDir = workingDir;
				processInfo.arguments = TC("-reuse");

				scheduler.EnqueueProcess(processInfo);
				scheduler.Start();

				u32 queued, active, finished;
				do { scheduler.GetStats(queued, active, finished); } while (queued != 0 || active != 0);

				scheduler.Stop();
				return true;
			});
	}

	bool TestRemoteScheduleReuse(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		if (!IsWindows)
			return true;

		return RunRemote(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{

				Scheduler scheduler(session, 0u, true);

				StringBuffer<> testApp;
				GetTestAppPath(logger, testApp);

				ProcessStartInfo processInfo;
				processInfo.application = testApp.data;
				processInfo.workingDir = workingDir;
				processInfo.arguments = TC("-reuse");

				scheduler.EnqueueProcess(processInfo);
				scheduler.Start();

				u32 queued, active, finished;
				do { scheduler.GetStats(queued, active, finished); } while (queued != 0 || active != 0);

				scheduler.Stop();
				return true;
			});
	}
}
