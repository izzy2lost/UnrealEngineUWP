// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaNetworkBackendTcp.h"
#include "UbaFileAccessor.h"
#include "UbaSessionServer.h"
#include "UbaStorageServer.h"
#include "UbaPlatform.h"
#include "UbaVersion.h"

#include "UbaAWS.h"

#if PLATFORM_WINDOWS
#include <dbghelp.h>
#include <io.h>
#pragma comment (lib, "Dbghelp.lib")
#endif

namespace uba
{

	const char*		Version = GetVersionString();
	u32				DefaultCapacityGb = 20;
	const tchar*	DefaultRootDir = []() {
		static tchar buf[256];
		if (IsWindows)
			ExpandEnvironmentStringsW(TC("%ProgramData%\\Epic\\" UE_APP_NAME), buf, sizeof(buf));
		else
			TStrcpy_s(buf, sizeof(buf), TC("~/" UE_APP_NAME));
		return buf;
		}();
	u32				DefaultProcessorCount = []() { return GetLogicalProcessorCount(); }();

	int PrintHelp(const tchar* message)
	{
		LoggerWithWriter logger(g_consoleLogWriter, TC(""));
		if (*message)
		{
			logger.Info(TC(""));
			logger.Error(TC("%s"), message);
		}
		logger.Info(TC(""));
		logger.Info(TC("------------------------"));
		logger.Info(TC("   UbaCli v%hs"), Version);
		logger.Info(TC("------------------------"));
		logger.Info(TC(""));
		logger.Info(TC("  UbaCli.exe [options...] <commandtype> <executable> [arguments...]"));
		logger.Info(TC(""));
		logger.Info(TC("  CommandTypes:"));
		logger.Info(TC("   local                   Will run executable locally using detoured paths"));
		logger.Info(TC("   remote                  Will wait for available agent and then run executable remotely"));
		logger.Info(TC("   native                  Will run executable in a normal way"));
		logger.Info(TC(""));
		logger.Info(TC("  Options:"));
		logger.Info(TC("   -dir=<rootdir>          The directory used to store data. Defaults to \"%s\""), DefaultRootDir);
		logger.Info(TC("   -port=[<host>:]<port>   The ip/name and port (default: %u) of the machine we want to help"), DefaultPort);
		logger.Info(TC("   -log                    Log all processes detouring information to file (only works with debug builds)"));
		logger.Info(TC("   -loop=<count>           Loop the commandline <count> number of times. Will exit when/if it fails"));
		logger.Info(TC("   -workdir=<dir>          Working directory"));
		logger.Info(TC("   -checkcas               Check so all cas entries are correct"));
		logger.Info(TC("   -checkaws               Check if we are inside aws and output information about aws"));
		logger.Info(TC("   -getcas                 Will print hash of application"));
		logger.Info(TC("   -summary                Print summary at the end of a session"));
		logger.Info(TC("   -nocustomalloc          Disable custom allocator for processes. If you see odd crashes this can be tested"));
		logger.Info(TC("   -storeraw               Disable compression of storage. This will use more storage and might improve performance"));
		if (IsWindows)
			logger.Info(TC("   -visualizer             Spawn a visualizer that visualizes progress"));
		logger.Info(TC(""));
		return -1;
	}

	StorageServer* g_storageServer;
	
	void CtrlBreakPressed()
	{
		if (g_storageServer)
		{
			g_storageServer->SaveCasTable(true);
			LoggerWithWriter(g_consoleLogWriter).Info(TC("CAS table saved..."));
		}
	}

	#if PLATFORM_WINDOWS
	BOOL ConsoleHandler(DWORD signal)
	{
		if (signal == CTRL_C_EVENT)
			CtrlBreakPressed();
		return FALSE;
	}
	#else
	void ConsoleHandler(int sig)
	{
		CtrlBreakPressed();
		exit(-1);
	}
	#endif
	
	StringBuffer<> g_rootDir(DefaultRootDir);

	//LONG WINAPI UbaUnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo)
	//{
	//	time_t rawtime;
	//	time(&rawtime);
	//	tm ti;
	//	localtime_s(&ti, &rawtime);
	//
	//	StringBuffer<> dumpFile;
	//	dumpFile.Append(g_rootDir).EnsureEndsWithSlash().Appendf(TC("UbaCliCrash_%02u%02u%02u_%02u%02u%02u.dmp"), ti.tm_year - 100, ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
	//
	//	wprintf(TC("Unhandled exception - Writing minidump %s\n"), dumpFile.data);
	//	HANDLE hFile = ::CreateFileW(dumpFile.data, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	//	MINIDUMP_EXCEPTION_INFORMATION mei;
	//	mei.ThreadId = GetCurrentThreadId();
	//	mei.ClientPointers = TRUE;
	//	mei.ExceptionPointers = ExceptionInfo;
	//	MiniDumpWriteDump(GetCurrentProcess(), ::GetCurrentProcessId(), hFile, MiniDumpNormal, &mei, NULL, NULL);
	//	return EXCEPTION_EXECUTE_HANDLER;
	//}


	int WrappedMain(int argc, tchar* argv[])
	{
		using namespace uba;
		//SetUnhandledExceptionFilter(UbaUnhandledExceptionFilter);

		u32 storageCapacityGb = DefaultCapacityGb;
		StringBuffer<> workDir;
		StringBuffer<> listenIp;
		u16 port = DefaultPort;
		bool launchVisualizer = false;
		bool storeCompressed = true;
		bool disableCustomAllocator = false;
		bool quiet = false;
		bool checkCas = false;
		bool checkAws = false;
		bool getCas = false;
		bool printSummary = false;
		u32 loopCount = 1;

		enum CommandType
		{
			CommandType_NotSet,
			CommandType_Local,
			CommandType_Remote,
			CommandType_Native,

		};

		CommandType commandType = CommandType_NotSet;

		TString application;
		TString arguments;

		for (int i=1; i!=argc; ++i)
		{
			StringBuffer<> name;
			StringBuffer<> value;

			if (const tchar* equals = TStrchr(argv[i],'='))
			{
				name.Append(argv[i], equals - argv[i]);
				value.Append(equals+1);
			}
			else
			{
				name.Append(argv[i]);
			}

			if (!application.empty())
			{
				if (!arguments.empty())
					arguments += ' ';
				bool hasSpace = TStrchr(argv[i], ' ');
				if (hasSpace)
					arguments += TC("\"");
				arguments += argv[i];
				if (hasSpace)
					arguments += TC("\"");
				continue;
			}
			if (commandType != CommandType_NotSet)
			{
				application = argv[i];
			}
			else if (name.Equals(TC("local")))
			{
				commandType = CommandType_Local;
			}
			else if (name.Equals(TC("remote")))
			{
				commandType = CommandType_Remote;
			}
			else if (name.Equals(TC("native")))
			{
				commandType = CommandType_Native;
			}
			else if (IsWindows && name.Equals(TC("-visualizer")))
			{
				launchVisualizer = true;
			}
			else if (name.Equals(TC("-workdir")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-workdir needs a value"));
				workDir.Append(value);
			}
			else if (name.Equals(TC("-capacity")))
			{
				if (!value.Parse(storageCapacityGb))
					return PrintHelp(TC("Invalid value for -capacity"));
			}
			else if (name.Equals(TC("-port")))
			{
				if (const tchar* portIndex = value.First(':'))
				{
					StringBuffer<> portStr(portIndex + 1);
					if (!portStr.Parse(port))
						return PrintHelp(TC("Invalid value for port in -port"));
					listenIp.Append(value.data, portIndex - value.data);
				}
				else
				{
					if (!value.Parse(port))
						return PrintHelp(TC("Invalid value for -port"));
				}
			}
			else if (name.Equals(TC("-loop")))
			{
				if (!value.Parse(loopCount))
					return PrintHelp(TC("Invalid value for -loop"));
			}
			else if (name.Equals(TC("-quiet")))
			{
				quiet = true;
			}
			else if (name.Equals(TC("-nocustomalloc")))
			{
				disableCustomAllocator = true;
			}
			else if (name.Equals(TC("-checkcas")))
			{
				checkCas = true;
			}
			else if (name.Equals(TC("-checkaws")))
			{
				checkAws = true;
			}
			else if (name.Equals(TC("-getcas")))
			{
				getCas = true;
			}
			else if (name.Equals(TC("-summary")))
			{
				printSummary = true;
			}
			else if (name.Equals(TC("-storeraw")))
			{
				storeCompressed = false;
			}
			else if (name.Equals(TC("-dir")))
			{
				if (value.IsEmpty())
					return PrintHelp(TC("-dir needs a value"));
				g_rootDir.Clear().Append(value);
			}
			else if (name.Equals(TC("-?")))
			{
				return PrintHelp(TC(""));
			}
			else
			{
				StringBuffer<> msg;
				msg.Appendf(TC("Unknown argument '%s'"), name.data);
				return PrintHelp(msg.data);
			}
		}

		FilteredLogWriter logWriter(g_consoleLogWriter, quiet ? LogEntryType_Info : LogEntryType_Detail);
		LoggerWithWriter logger(logWriter, TC(""));

		if (checkCas)
		{
			StorageCreateInfo storageInfo(g_rootDir.data, logWriter);
			storageInfo.casCapacityBytes = 0;
			storageInfo.storeCompressed = storeCompressed;
			StorageImpl storage(storageInfo);
			bool success = storage.CheckCasContent(DefaultProcessorCount);
			return success ? 0 : -1;
		}

		if (checkAws)
		{
			AWS aws;
			StringBuffer<> info;
			if (aws.Init(logger, info, TC("UbaCli")))
			{
				logger.Info(TC("We are inside AWS: %s (%s)"), info.data, aws.GetAvailabilityZone());
				
				StringBuffer<> reason;
				u64 terminateTime;
				if (aws.IsTerminating(logger, reason, terminateTime))
					logger.Info(TC(".. and are being terminated: %s"), reason.data);
			}
			else
				logger.Info(TC("Seems like we are not running inside aws."));
			return 0;
		}

		if (commandType == CommandType_NotSet)
		{
			const tchar* errorMsg = argc == 1 ? TC("") : TC("\nERROR: First argument must be command type. Options are 'local,remote or native'");
			StringBuffer<> msg;
			return PrintHelp(errorMsg);
		}

		StringBuffer<512> currentDir;
		GetCurrentDirectoryW(currentDir);

		bool isAbsolute = IsWindows ? application[1] == ':' : application[0] == '/';
		if (!isAbsolute)
		{
			StringBuffer<> fullApplicationName;
			if (!SearchPathForFile(logger, fullApplicationName, application.c_str(), currentDir.data))
				return logger.Error(TC("Failed to find full path to %s"), application.c_str());
			application = fullApplicationName.data;
		}

		if (getCas)
		{
			CasKey key;

			FileAccessor fa(logger, application.c_str());
			if (!fa.OpenMemoryRead())
				return logger.Error(TC("Failed to open file %s"), application.c_str());
			u64 fileSize = fa.GetSize();
			u8* data = fa.GetData();
			bool is64Bit = true;

			CasKeyHasher hasher;
			hasher.Update(data, fileSize);
			key = ToCasKey(hasher, false);

			if (data[0] != 'M' || data[1] != 'Z')
				is64Bit = false;
			else
			{
				u32 offset = *(u32*)(data + 0x3c);
				is64Bit = *(u32*)(data + offset) == 0x00004550;
			}
			logger.Info(TC("%s"), application.c_str());
			logger.Info(TC("  Is64Bit: %s"), (is64Bit ? TC("true") : TC("false")));
			logger.Info(TC("  Size: %llu"), fileSize);
			logger.Info(TC("  CasKey: %s"), CasKeyString(key).str);
			return 0;
		}

		const tchar* dbgStr = TC("");
		#if UBA_DEBUG
		dbgStr = TC(" (DEBUG)");
		#endif
		logger.Info(TC("UbaCli v%hs%s (Rootdir: \"%s\", StoreCapacity: %uGb)\n"), Version, dbgStr, g_rootDir.data, storageCapacityGb);

		u64 storageCapacity = u64(storageCapacityGb)*1000*1000*1000;

		if (workDir.IsEmpty())
			workDir.Append(currentDir);

		StringBuffer<> logFile;
		logFile.count = GetFullPathNameW(g_rootDir.data, logFile.capacity, logFile.data, nullptr);
		logFile.EnsureEndsWithSlash().Append(TC("DebugLog.log"));

		#if PLATFORM_WINDOWS
		SetConsoleCtrlHandler(ConsoleHandler, TRUE);
		#else
		signal(SIGINT, ConsoleHandler);
		#endif

		NetworkBackendTcp networkBackend(logWriter);
		NetworkServerCreateInfo nsci(logWriter);
		bool ctorSuccess = true;
		NetworkServer* server = new NetworkServer(ctorSuccess, nsci);
		auto destroyServer = MakeGuard([&]() { delete server; });
		if (!ctorSuccess)
			return -1;

		StorageServerCreateInfo storageInfo(*server, g_rootDir.data, logWriter);
		storageInfo.casCapacityBytes = storageCapacity;
		storageInfo.storeCompressed = storeCompressed;
		StorageServer* storage = new StorageServer(storageInfo);
		auto destroyStorage = MakeGuard([&]() { delete storage; });

		SessionServerCreateInfo info(*storage, *server);
		info.useUniqueId = false;
		info.launchVisualizer = launchVisualizer;
		info.disableCustomAllocator = disableCustomAllocator;
		//info.shouldWriteToDisk = shouldWriteToDisk;
		info.rootDir = g_rootDir.data;
		//info.traceName.Append(TC("TESTTRACE"));
		info.deleteSessionsOlderThanSeconds = 1;
		auto session = new SessionServer(info);
		auto destroySession = MakeGuard([&]() { delete session; });

		if (commandType == CommandType_Remote)
		{
			if (!storage->LoadCasTable(true))
				return -1;
			if (!server->StartListen(networkBackend, port, listenIp.data))
				return -1;
		}
		auto g = MakeGuard([&]() { server->StopAll(); });

		auto RunBoxed = [&](const TString& app, const TString& arg, bool trackInputs = false)
		{
			u64 start = GetTime();
			ProcessStartInfo pinfo;
			pinfo.application = app.c_str();
			pinfo.arguments = arg.c_str();
			pinfo.workingDir = workDir.data;
			pinfo.logFile = logFile.data;
			pinfo.logLineUserData = &logger;
			pinfo.logLineFunc = [](void* userData, const tchar* line, u32 length, LogEntryType type) { ((Logger*)userData)->Log(type, line, length); };
			pinfo.trackInputs = trackInputs;
			logger.Info(TC("Running %s %s"), app.c_str(), arg.c_str());
			ProcessHandle process = session->RunProcess(pinfo, false);
			if (process.GetExitCode() != 0)
				return logger.Error(TC("Error exit code: %u"), process.GetExitCode());
			u64 time = GetTime() - start;
			logger.Info(TC("Boxed run took %s"), TimeToText(time).str);
			return true;
		};

		auto RunNormal = [&](const TString& app, const TString& arg)
		{
			#if PLATFORM_WINDOWS
			u64 start = GetTime();
			STARTUPINFOW si;
			memset(&si, 0, sizeof(si));
			PROCESS_INFORMATION pi;
			TString cmdLine = app + TC(" ") + arg;
			logger.Info(TC("Running %s"), cmdLine.c_str());
			if (!CreateProcessW(NULL, (tchar*)cmdLine.c_str(), NULL, NULL, false, 0, NULL, workDir.data, &si, &pi))
				return logger.Error(TC("Failed to run %s (%s)"), cmdLine.c_str(), LastErrorToText().data);
			::CloseHandle(pi.hThread);
			WaitForSingleObject(pi.hProcess, INFINITE);
			DWORD exitCode = 0;
			GetExitCodeProcess(pi.hProcess, &exitCode);
			if (exitCode != 0)
				return logger.Error(TC("Error exit code: %u"), exitCode);
			::CloseHandle(pi.hProcess);
			u64 time = GetTime() - start;
			logger.Info(TC("Normal run took %s"), TimeToText(time).str);
			return true;
			#else
			return logger.Error(TC("Normal run only implement on windows right now"));
			#endif
		};

		auto RunBoxedRemote = [&](const TString& app, const TString& arg)
		{
			u64 start = GetTime();
			ProcessStartInfo pinfo;
			pinfo.application = app.c_str();
			pinfo.arguments = arg.c_str();
			pinfo.workingDir = workDir.data;
			pinfo.logFile = logFile.data;
			pinfo.logLineUserData = &logger;
			pinfo.logLineFunc = [](void* userData, const tchar* line, u32 length, LogEntryType type) { ((Logger*)userData)->Log(type, line, length); };
			logger.Info(TC("Running %s %s"), app.c_str(), arg.c_str());
			ProcessHandle process = session->RunProcessRemote(pinfo);
			process.WaitForExit(~0u);
			if (process.GetExitCode() != 0)
				return logger.Error(TC("Error exit code: %u"), process.GetExitCode());
			u64 time = GetTime() - start;
			logger.Info(TC("Remote run took %s"), TimeToText(time).str);
			return true;
		};

		for (u32 i=0; i!=loopCount; ++i)
		{
			bool success = false;
			switch (commandType)
			{
			case CommandType_Native:
				success = RunNormal(application, arguments);
				break;
			case CommandType_Local:
				success = RunBoxed(application, arguments);
				break;
			case CommandType_Remote:
				success = RunBoxedRemote(application, arguments);
				break;
			}
			if (!success)
				return -1;
		}

		logger.BeginScope();
		if (printSummary)
		{
			session->PrintSummary(logger);
			storage->PrintSummary(logger);
			server->PrintSummary(logger);
			SystemStats::GetGlobal().Print(logger, true);
		}
		logger.EndScope();

		return 0;
	}
}

#if PLATFORM_WINDOWS
int wmain(int argc, wchar_t* argv[])
{
	return uba::WrappedMain(argc, argv);
}
#else
int main(int argc, char* argv[])
{
	return uba::WrappedMain(argc, argv);
}
#endif
