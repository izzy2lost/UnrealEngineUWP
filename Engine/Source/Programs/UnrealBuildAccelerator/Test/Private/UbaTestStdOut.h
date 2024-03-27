// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaSessionServer.h"
#include "UbaStorageServer.h"

namespace uba
{
	bool TestStdOut(LoggerWithWriter& logger, const StringBufferBase& testRootDir, bool remote)
	{
		LogWriter& logWriter = logger.m_writer;
		NetworkBackendTcp networkBackend(logWriter);

		bool ctorSuccess = true;
		NetworkServer networkServer(ctorSuccess, { logWriter });

		StringBuffer<> rootDir;
		rootDir.Append(testRootDir).Append(TC("Uba"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		StorageServerCreateInfo storageServerInfo(networkServer, rootDir.data, logWriter);
		storageServerInfo.casCapacityBytes = 1024ull * 1024 * 1024;
		StorageServer storageServer(storageServerInfo);

		SessionServerCreateInfo sessionServerInfo(storageServer, networkServer, logWriter);
		sessionServerInfo.checkMemory = false;
		sessionServerInfo.rootDir = rootDir.data;
		sessionServerInfo.traceEnabled = true;
		//sessionServerInfo.remoteLogEnabled = true;
		SessionServer sessionServer(sessionServerInfo);

		auto sg = MakeGuard([&]() { networkServer.DisconnectClients(); });

		StringBuffer<> workingDir;
		workingDir.Append(testRootDir).Append(TC("WorkingDir"));
		if (!DeleteAllFiles(logger, workingDir.data))
			return false;
		if (!storageServer.CreateDirectory(workingDir.data))
			return false;
		if (!DeleteAllFiles(logger, workingDir.data, false))
			return false;
		workingDir.EnsureEndsWithSlash();

		if (!networkServer.StartListen(networkBackend))
			return logger.Error(TC("Failed to listen"));

		StringBuffer<> testApp;
		GetTestAppPath(logger, testApp);

		ProcessStartInfo pi;
		pi.application = testApp.data;
		pi.arguments = TC("-stdout=rootprocess");
		pi.workingDir = workingDir.data;
		pi.description = TC("StdOutDesc");
		pi.logFile = TC("Log");

		auto ph = remote ? sessionServer.RunProcessRemote(pi) : sessionServer.RunProcess(pi);
		if (!ph.WaitForExit(5000))
			return logger.Error(TC("Timed out waiting for process"));
		if (auto ec = ph.GetExitCode())
			return logger.Error(TC("Process exited with error code %u"), ec);

		networkBackend.StopListen();
		networkServer.DisconnectClients();
		sessionServer.WaitOnAllTasks();

		auto& logLines = ph.GetLogLines();
		if (logLines.size() != 1)
			return false;
		if (!Equals(logLines[0].text.c_str(), TC("rootprocess")))
			return false;

		return true;
	}

	bool TestStdOutLocal(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return TestStdOut(logger, testRootDir, false);
	}

	bool TestStdOutRemote(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return TestStdOut(logger, testRootDir, true);
	}
}
