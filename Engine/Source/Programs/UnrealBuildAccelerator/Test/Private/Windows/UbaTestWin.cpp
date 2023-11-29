// Copyright Epic Games, Inc. All Rights Reserved.

#include <iostream>
#include "UbaNetworkClient.h"
#include "UbaNetworkServer.h"
#include "UbaSessionClient.h"
#include "UbaSessionServer.h"
#include "UbaStorageClient.h"
#include "UbaStorageServer.h"
#include "UbaStorageProxy.h"
#include "UbaProcess.h"
#include "UbaNetworkBackendQuic.h"
#include "UbaNetworkBackendTcp.h"
#include "UbaTestAll.h"

int wmain(int argc, wchar_t* argv[])
{
	using namespace uba;

	if (!RunAllTests())
		return -1;
#if 0
	LogWriter& logWriter = g_consoleLogWriter;
	LoggerWithWriter logger(logWriter, L"");

	TString rootDir(L"e:\\temp\\Box");

	TString workingDir = L"E:\\dev\\fn\\Engine\\Source";

	TString clientRootDir = L"e:\\temp\\BoxClient";
	TString serverRootDir = L"e:\\temp\\BoxServer";

	TString winSdkDir = L"c:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22000.0\\x64";
	//TString msvcBinDir = L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\bin\\Hostx64\\X64\\";
	TString msvcBinDir = L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.37.32822\\bin\\Hostx64\\X64\\";
	//TString msvcBinDir = L"c:\\sdk\\AutoSDK\\HostWin64\\Win64\\VS2022-Experimental\\14.37.32823\\bin\\Hostx64\\x64\\";
	//TString msvcBinDir = L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Preview\\VC\\Tools\\MSVC\\14.38.33030\\bin\\Hostx64\\X64\\";

	//TString linuxSysRoot = L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\x64";
	TString linuxSysRoot = L"c:/sdk/AutoSDK/HostWin64/Linux_x64/v21_clang-15.0.1-centos7/x86_64-unknown-linux-gnu";
	//TString linuxSysRoot = L"e:\\dev\\fn\\Engine\\Source\\ThirdParty\\IWYU\\llvm1501\\build";
	TString linuxBinDir = linuxSysRoot + L"/bin/";

	TString androidBinDir = L"c:\\sdk\\autosdk\\hostwin64\\android\\-26\\ndk\\25.1.8937393\\toolchains\\llvm\\prebuilt\\windows-x86_64\\bin\\";
	TString androidCompileExe = androidBinDir + L"clang++.exe";

	TString msvcCompileExe = msvcBinDir + L"cl.exe";
	TString winClangCompileExe = linuxBinDir + L"clang-cl.exe";
	TString linuxCompileExe = linuxBinDir + L"clang++.exe";

	TString win64CompileExe = msvcCompileExe;
	//TString win64CompileExe = winClangCompileExe;

	TString win64LinkExe = msvcBinDir + L"link.exe";

	bool serverStoreCompressed = true; // Will both store and send compressed if true.. 
	bool clientStoreCompressed = true; // Will store received files compressed on disk (with a few exceptions, which is binaries)
	bool clientSendCompressed = true; // Will compress file when sent to server
	bool launchVisualizer = false;
	bool shouldWriteToDisk = true;

	bool compileWin64 = false;
	bool compileLinux = false;
	bool compileAndroid = false;
	bool linkWin64 = false;
	bool libWin64 = false;
	bool linkLinux = false;
	bool copy = false;
	bool xcopy = false;
	bool writeMeta = false;
	bool rc = false;
	bool batch = false;
	bool customCasKey = false;
	bool compileClientServerSocket = true;
	//bool compileClientServerNamedEvents = false;
	bool deleteCas = true;
	bool logToFile = true;
	bool useCrypto = true;

	u8 cryptoKeyData[16] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };
	u8* cryptoKey = useCrypto ? cryptoKeyData : nullptr;

	{
		StringBuffer<64*1024> env;
		env.count = uba::GetEnvironmentVariableW(L"PATH", env.data, 64*1024);
		env.Append(L";").Append(winSdkDir).Append(L";").AppendDir(StringBuffer<>(win64CompileExe));
		SetEnvironmentVariableW(L"PATH", env.data);
	}

	int sendSize = SendDefaultSize;// 64 * 1024;
	//for (int sendSize=4224; sendSize!=250000; ++sendSize)
	//while (true)
	{
		if (sendSize != SendDefaultSize)
			logger.Info(L"Uba message send size: %u", sendSize);
		#if UBA_USE_QUIC
		NetworkBackendQuic serverBackend(logWriter);
		#else
		NetworkBackendTcp serverBackend(logWriter);
		#endif
		NetworkServerCreateInfo nsci(logWriter);
		nsci.sendSize = sendSize;
		bool ctorSuccess = true;
		NetworkServer* server = new NetworkServer(ctorSuccess, nsci);
		auto destroyServer = MakeGuard([&]() { delete server; });

		StorageServerCreateInfo storageInfo(*server, rootDir.c_str(), logWriter);
		storageInfo.casCapacityBytes = 0;
		storageInfo.storeCompressed = serverStoreCompressed;
		StorageServer* storage = new StorageServer(storageInfo);
		auto destroyStorage = MakeGuard([&]() { delete storage; });

		SessionServerCreateInfo info(*storage, *server);
		info.useUniqueId = false;
		info.launchVisualizer = launchVisualizer;
		info.shouldWriteToDisk = shouldWriteToDisk;
		info.logToFile = logToFile;
		info.rootDir = rootDir.c_str();
		info.traceName = L"TESTTRACE";
		info.deleteSessionsOlderThanSeconds = 1;
		info.detailedTrace = true;
		auto session = new SessionServer(info);
		auto destroySession = MakeGuard([&]() { delete session; });

		auto RunBoxed = [&](const TString& app, const TString& arg, bool trackInputs = false) -> ProcessHandle
		{
			u64 start = GetTime();
			ProcessStartInfo pinfo;
			pinfo.application = app.c_str();
			pinfo.arguments = arg.c_str();
			pinfo.workingDir = workingDir.c_str();
			pinfo.logLineUserData = &logger;
			pinfo.logLineFunc = [](void* userData, const wchar_t* line, u32 length) { ((Logger*)userData)->Log(LogEntryType_Info, line, length); };
			pinfo.trackInputs = trackInputs;
			ProcessHandle process = session->RunProcess(pinfo, false);
			if (process.GetExitCode() != 0)
				logger.Error(L"Error exit code: %i", process.GetExitCode());
			u64 time = GetTime() - start;
			logger.Info(L"Boxed run took %ls", TimeToText(time).str);
			return process;
		};

		auto RunBoxed2 = [&](const TString& app, Vector<const wchar_t*> args) -> bool
		{
			u64 start = GetTime();
			ProcessStartInfo pinfo;
			pinfo.application = app.c_str();
			pinfo.workingDir = workingDir.c_str();
			pinfo.logLineUserData = &logger;
			pinfo.logLineFunc = [](void* userData, const wchar_t* line, u32 length) { ((Logger*)userData)->Log(LogEntryType_Info, line, length); };
			Vector<ProcessHandle> handles;
			for (auto arg : args)
			{
				pinfo.arguments = arg;
				handles.push_back(session->RunProcess(pinfo, true));
			}

			for (auto& h : handles)
			{
				h.WaitForExit(2000000);
				if (h.GetExitCode() != 0)
				{
					logger.Error(L"Error exit code: %i", h.GetExitCode());
					return false;
				}
			}
			u64 time = GetTime() - start;
			logger.Info(L"Boxed run took %ls", TimeToText(time).str);
			return true;
		};

		auto RunNormal = [&](const TString& app, const TString& arg)
		{
			u64 start = GetTime();
			STARTUPINFOW si;
			memset(&si, 0, sizeof(si));
			PROCESS_INFORMATION pi;
			TString cmdLine = app + L" " + arg;
			CreateProcessW(NULL, (wchar_t*)cmdLine.c_str(), NULL, NULL, false, 0, NULL, workingDir.c_str(), &si, &pi);
			CloseHandle(pi.hThread);
			WaitForSingleObject(pi.hProcess, INFINITE);
			DWORD exitCode;
			GetExitCodeProcess(pi.hProcess, &exitCode);
			if (exitCode != 0)
				logger.Error(L"Error exit code: %i", exitCode);
			CloseHandle(pi.hProcess);
			u64 time = GetTime() - start;
			logger.Info(L"Normal run took %ls", TimeToText(time).str);
		};

		auto RunBoxedRemote = [&](const TString& app, const TString& arg)
		{
			ProcessStartInfo pinfo;
			pinfo.application = app.c_str();
			pinfo.arguments = arg.c_str();
			pinfo.workingDir = workingDir.c_str();
			pinfo.logLineUserData = &logger;
			pinfo.logLineFunc = [](void* userData, const wchar_t* line, u32 length) { ((Logger*)userData)->Log(LogEntryType_Info, line, length); };
			u64 start = GetTime();
			ProcessHandle process = session->RunProcessRemote(pinfo);
			process.WaitForExit(INFINITE);
			u64 time = GetTime() - start;
			if (process.GetExitCode() != 0)
				logger.Error(L"Error exit code: %i", process.GetExitCode());
			logger.Info(L"Remote run took %ls", TimeToText(time).str);
		};
		
		//-Xclang -ftime-trace

		//RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\x64\\bin\\clang-cl.exe", L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditorFortnite\\Development\\MuCOE\\Module.MuCOE.6.cpp.obj.rsp");
		//RunNormal(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\x64\\bin\\clang-cl.exe", L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditorFortnite\\Development\\MuCOE\\Module.MuCOE.6.cpp.obj.rsp");

		//RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\x64\\bin\\clang-cl.exe", L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditorFortnite\\Development\\MuCOE\\Module.MuCOE.5.cpp.obj.rsp -Xclang -ftime-trace");
		//RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\x64\\bin\\clang-cl.exe", L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditorFortnite\\Development\\ControlRigEditor\\Module.ControlRigEditor.3.cpp.obj.rsp -Xclang -ftime-trace");
		//RunNormal(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\x64\\bin\\clang-cl.exe", L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditorFortnite\\Development\\MuCOE\\Module.MuCOE.6.cpp.obj.rsp -Xclang -ftime-trace");

		//u32 count;
		//DeleteAllFiles(logger, L"e:\\dev\\fn\\Engine\\Saved\\Config", count, true);
		//RunBoxed(L"E:\\dev\\fn\\Engine\\Platforms\\XboxCommon\\Binaries\\Win64\\XboxPDBFileUtil.exe", L"E:\\dev\\fn\\Engine\\Binaries\\XSX\\UnrealGame-BuildSettings.dll E:\\dev\\fn\\Engine\\Build\\XSX\\Symbols\\UnrealGame-BuildSettings-symbols.bin -NODEFAULTLOG");
		//RunBoxed(L"E:\\dev\\fn\\Engine\\Binaries\\Linux\\BreakpadSymbolEncoder.exe", L"E:\\dev\\fn\\FortniteGame\\Binaries\\Linux\\FortniteServerVkEdit-Linux-Shipping.psym E:\\dev\\fn\\FortniteGame\\Binaries\\Linux\\FortniteServerVkEdit-Linux-Shipping.sym");
		//return 0;

		//workingDir = L"E:\\dev\\fn\\Engine\\Source\\Programs\\UnrealBuildTool";
		//RunBoxed(L"e:\\dev\\fn\\Engine\\Build\\BatchFiles\\..\\..\\Binaries\\ThirdParty\\DotNet\\6.0.302\\windows\\dotnet.exe", L"build Programs\\UnrealBuildTool\\UnrealBuildTool.csproj -c Development -v quiet");

		//RunBoxed(L"e:\\dev\\fn\\engine\\Build\\BatchFiles\\BuildUBT.bat", L"");
		//RunBoxed(L"e:\\dev\\fn\\GenerateProjectFiles.bat", L"e:\\dev\\fn\\FortniteGame\\FortniteGame.uproject");

		//RunBoxed(L"C:\\WINDOWS\\system32\\cmd.exe", L"/c e:\\dev\\fn\\GenerateProjectFiles.bat e:\\dev\\fn\\FortniteGame\\FortniteGame.uproject");
		//workingDir = L"e:\\temp";
		//RunBoxed(L"e:\\temp\\TestBatch.bat", L"");
		//return 0;

		//RunBoxed(L"e:\\dev\\fn\\Engine\\Platforms\\XboxCommon\\Binaries\\Win64\\XboxPDBFileUtil.exe", L"e:\\dev\\fn\\FortniteGame\\Binaries\\XSX\\FortniteClient.exe e:\\dev\\fn\\FortniteGame\\Build\\XSX\\Symbols\\FortniteClient-symbols.bin");

		//RunBoxed(L"e:\\dev\\fn\\Engine\\Binaries\\Win64\\UnrealEditor.exe", L"FortniteGame -DisablePython");

		//RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.37.32822\\bin\\Hostx64\\x64\\pgomgr.exe", L"/merge /nologo \"E:\\dev\\fn\\FortniteGame\\Binaries\\Win64\\FortniteClient-Win64-Shipping.pgd");
		//RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.37.32822\\bin\\Hostx64\\x64\\link.exe", L"@\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgram\\Shipping\\BlankProgram-Win64-Shipping.exe.rsp\"");
		//RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.37.32822\\bin\\Hostx64\\x64\\link.exe", L"@\"E:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\FortniteClient\\Shipping\\FortniteClient-Win64-Shipping.exe.rsp\"");


		if (compileWin64)
		{
			logger.Info(L"Testing compile msvc");
			//TString args = L"\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\VisualStudioDTE\\dte80a.cpp\" /c /nologo /Fo\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\VisualStudioDTE\\dte80a.obj\" /I . /external:W0 /external:I \"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\INCLUDE\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\ucrt\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\shared\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\um\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\winrt\"";
			//TString args = L"\"e:\\dev\\fn\\Engine\\Plugins\\Runtime\\Database\\ADOSupport\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ADOSupport\\msado15.cpp\" /c /nologo /Fo\"e:\\dev\\fn\\Engine\\Plugins\\Runtime\\Database\\ADOSupport\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ADOSupport\\msado15.obj\" /I . /external:W0 /external:I \"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\INCLUDE\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\ucrt\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\shared\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\um\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\winrt\"";
			//TString args = L"@e:\\dev\\fn\\engine\\intermediate\\build\\win64\\x64\\blankprogramnonunity\\development\\core\\SharedPCH.Core.ShadowErrors.h.obj.rsp";
			TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core\\PCH.Core.h.obj.rsp";
			//TString args = L"@..\\Plugins\\BoxController\\Intermediate\\Build\\Win64\\x64\\UnrealEditorNU\\Development\\BoxController\\BoxJobProcessor.cpp.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\ApplicationCore\\WindowsUIAManager.cpp.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\FortniteGame\\PCH.FortniteGame.h.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNU\\Development\\Core\\Stats.cpp.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNU\\Development\\TraceLog\\Tail.cpp.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealServer\\Development\\Chaos\\Module.Chaos.1.cpp.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core\\MiMalloc.c.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\fortnitegame\\intermediate\\build\\xsx\\x64\\fortniteclient\\shipping\\resonanceaudio\\ambisonic_lookup_table.cc.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Plugins\\Runtime\\AR\\Google\\GoogleARCore\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\GoogleARCoreBase\\GoogleARCoreDependencyHandler.cpp.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Json\\JsonModule.cpp.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\FortniteGame\\Module.FortniteGame.127_of_150.cpp.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\BuildSettings\\BuildSettings.cpp.obj.rsp";
			//TString args = L"@..\\Plugins\\Experimental\\NNE\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\NNEORT\\noop_elimination.cc.obj.rsp";
			//TString args = L"@e:\\dev\\fn\\engine\\Intermediate\\Build\\Win64\\x64\\BlankProgram\\Development\\Core\\Module.Core.1_of_17.cpp.obj.rsp";
			//RunNormal(win64CompileExe, args);
			RunBoxed(win64CompileExe, args);
			//RunBoxed(win64CompileExe, args);
			//RunBoxed(win64CompileExe, args);
			//RunBoxed(win64CompileExe, args);

			//for (int i=0;i!=10; ++i)
			//	RunBoxed(win64CompileExe, args);

			//session->RefreshDirectory(L"e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core");

			//RunBoxed(win64CompileExe, args);

			//TString args2 = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\BuildSettings\\UnrealPakNU-BuildSettings.dll.rsp";
			//RunBoxed(msvcBinDir + L"link.exe", args2);
		}

		if (compileLinux)
		{
			logger.Info(L"Testing compile linux (clang)");
			//TString args = L"@e:\\dev\\fn\\engine\\Intermediate\\Build\\Linux\\x64\\BlankProgram\\Development\\Core\\PCH.Core.h.gch.rsp";
			TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\BlankProgramNonUnity\\Development\\Core\\ABTesting.cpp.o.rsp";
			//TString args = L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Linux\\x64\\UnrealEditor\\Development\\FortniteUI\\Module.FortniteUI.12_of_45.cpp.o.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\UnrealPakNU\\Development\\ColorManagement\\TransferFunctions.cpp.o.rsp";
			//TString args = L"@../Intermediate/Build/Linux/x64/UnrealPakNU/Development/CoreUObject/SharedPCH.CoreUObject.ShadowErrors.h.gch.rsp";

			//for (int i=0;i!=8;++i)
			{
				//RunNormal(linuxCompileExe, args);
				RunBoxed(linuxCompileExe, args);
			}
		}

		if (compileAndroid)
		{
			RunBoxed(androidCompileExe, L"@../Intermediate/Build/Android/a/UnrealGame/Development/GoogleOboe/Utilities.cpp.o.rsp");
		}

		//if (true)
		//{
		//	TString args = L"-Xiwyu --mapping_file=E:\\dev\\fn\\Engine\\Binaries\\ThirdParty\\IWYU\\ue_mapping.imp -Xiwyu --prefix_header_includes=keep -Xiwyu --no_check_matching_header -Xiwyu --cxx17ns -Xiwyu --write_json_path=\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\iwyu\\BlankProgramNonUnity\\Development\\Core\\LowLevelTestAdapter.h.iwyu\" @\"E:/dev/fn/Engine/Intermediate/Build/Linux/iwyu/BlankProgramNonUnity/Development/Core/LowLevelTestAdapter.h.iwyu.rsp\"";
		//	RunBoxed(L"E:\\dev\\fn\\Engine\\Binaries\\ThirdParty\\IWYU\\include-what-you-use.exe", args);
		//}

		//if (true)
		//{
		//	TString exe = L"C:\\sdk\\AutoSDK\\HostWin64\\Switch\\15.3.0_4.6.7\\NintendoSDK\\Compilers\\NX\\nx\\aarch64\\bin\\ld.lld.exe";
		//	TString args = L"@e:\\temp\\box\\sessions\\230430_055933\\temp\\response-95b56d.txt";
		//	RunBoxed(exe, args);
		//}

		if (linkWin64)
		{
			logger.Info(L"Testing link msvc");
			TString exe = win64LinkExe;
			TString args = L"@E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\TraceLog\\UnrealPak-TraceLog.dll.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\BlankProgramNonUnity.exe.rsp";
			//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\SlateCore\\UnrealEditor-SlateCore.dll.rsp";
			if (RunBoxed(exe, args).GetExitCode() != 0)
				return 0;
			#if 0
			if (!RunBoxed2(exe,
				{
					L"@E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\TraceLog\\UnrealPak-TraceLog.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\TraceLog\\UnrealEditor-TraceLog.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\SlateCore\\UnrealEditor-SlateCore.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Slate\\UnrealEditor-Slate.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\SkeletonEditor\\UnrealEditor-SkeletonEditor.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\RHI\\UnrealEditor-RHI.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Renderer\\UnrealEditor-Renderer.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\RenderCore\\UnrealEditor-RenderCore.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\RemoteExecution\\UnrealEditor-RemoteExecution.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\PropertyEditor\\UnrealEditor-PropertyEditor.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ProfilerClient\\UnrealEditor-ProfilerClient.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\PakFile\\UnrealEditor-PakFile.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\OverlayEditor\\UnrealEditor-OverlayEditor.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Networking\\UnrealEditor-Networking.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\NetCore\\UnrealEditor-NetCore.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\NetCommon\\UnrealEditor-NetCommon.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Navmesh\\UnrealEditor-Navmesh.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\NaniteUtilities\\UnrealEditor-NaniteUtilities.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MRMesh\\UnrealEditor-MRMesh.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MovieSceneTools\\UnrealEditor-MovieSceneTools.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MovieScene\\UnrealEditor-MovieScene.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Messaging\\UnrealEditor-Messaging.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MessageLog\\UnrealEditor-MessageLog.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MeshUtilities\\UnrealEditor-MeshUtilities.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MeshPaint\\UnrealEditor-MeshPaint.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MeshBuilder\\UnrealEditor-MeshBuilder.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MaterialEditor\\UnrealEditor-MaterialEditor.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Localization\\UnrealEditor-Localization.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\LiveCoding\\UnrealEditor-LiveCoding.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\LevelEditor\\UnrealEditor-LevelEditor.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Chaos\\UnrealEditor-Chaos.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ChaosCore\\UnrealEditor-ChaosCore.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ChaosVehiclesCore\\UnrealEditor-ChaosVehiclesCore.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\CinematicCamera\\UnrealEditor-CinematicCamera.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ClothingSystemEditor\\UnrealEditor-ClothingSystemEditor.dll.rsp",
					L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ClothPainter\\UnrealEditor-ClothPainter.dll.rsp",
				}))
				return 0;
			#endif
		}

		if (libWin64)
		{
			logger.Info(L"Testing link msvc");
			TString exe = msvcBinDir + L"lib.exe";
			TString args = L"@E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\NotForLicensees\\uLangCore\\UnrealEditor-uLangCore.lib.rsp";
			RunBoxed(exe, args);
		}

		if (linkLinux)
		{
			TString exe = L"C:\\WINDOWS\\system32\\cmd.exe";
			//TString exeArgs = L"/C E:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\UnrealPakNU\\Development\\Link-libUnrealPakNU-TraceLog.so.link.bat";
			//TString exeArgs = L"/C e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\BlankProgram\\Development\\Link-BlankProgram.link.bat";
			//TString exeArgs = L"/C e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\UnrealEditor\\Development\\Link-libUnrealEditor-UMGEditor.so.link.bat";
			TString exeArgs = L"/C e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\UnrealEditor\\Development\\Link-libUnrealEditor-TranslationEditor.so.link.bat";
			RunBoxed(exe, exeArgs);
		}

		if (copy)
		{
			logger.Info(L"Testing copy");
			TString exe = L"C:\\WINDOWS\\system32\\cmd.exe";
			TString exeArgs = L"/C \"copy /Y \"E:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Linux\\x64\\FortniteClient\\Development\\Chaos\\PBDEvolution.ispc.generated.dummy.h\" \"E:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Linux\\x64\\FortniteClient\\Development\\Chaos\\PBDEvolution.ispc.generated.h\" 1>nul\"";
			//TString exeArgs = L"/C \"copy /Y \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Chaos\\PBDMinEvolution.ispc.generated.dummy.h\" \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Chaos\\PBDMinEvolution.ispc.generated.h\" 1>nul\"";
			//RunNormal(exe, exeArgs, workingDir);
			RunBoxed(exe, exeArgs);
		}

		if (xcopy)
		{
			logger.Info(L"Testing xcopy");
			TString exe = L"C:\\Windows\\system32\\xcopy.exe";
			TString args = L"/s E:\\dev\\fn\\Engine\\Intermediate\\Build\\Switch\\HtmlDocument_Staging\\Development\\*.* \"E:\\dev\\fn\\Engine\\Binaries\\Switch\\UnrealClient.nspd\\htmlDocument0.ncd\\data\\html-document\"";
			RunBoxed(exe, args);
		}

		if (writeMeta)
		{
			logger.Info(L"Testing writemeta");
			//TString exeArgs1 = L"\"E:\\dev\\fn\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.dll\" -Mode=WriteMetadata -Input=E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\UnrealPakNU\\Development\\EngineMetadata.dat -Version=2";
			//TString exeArgs2 = L"\"E:\\dev\\fn\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.dll\" -Mode=WriteMetadata -Input=E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\UnrealPakNU\\Development\\TargetMetadata.dat -Version=2";
			TString exeArgs1 = L"E:\\dev\\fn\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.dll -Mode=WriteMetadata -Input=e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\TargetMetadata.dat -Version=2";
			TString exe = L"E:\\dev\\fn\\Engine\\Binaries\\ThirdParty\\DotNet\\6.0.302\\windows\\dotnet.exe";
			RunBoxed(exe, exeArgs1);
			//RunBoxed(exe, exeArgs2);
		}

		if (rc)
		{
			logger.Info(L"Testing rc");
			TString exe = winSdkDir + L"\\rc.exe";
			TString exeArgs = L"/nologo /D_WIN64 /l 0x409 /I \".\" /I \"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\INCLUDE\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\ucrt\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\shared\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\um\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\winrt\" /DIS_PROGRAM=0 /DUE_EDITOR=1 /DENABLE_PGO_PROFILE=0 /DUSE_VORBIS_FOR_STREAMING=1 /DUSE_XMA2_FOR_STREAMING=1 /DWITH_DEV_AUTOMATION_TESTS=1 /DWITH_PERF_AUTOMATION_TESTS=1 /DWITH_LOW_LEVEL_TESTS=0 /DWITH_TESTS=1 /DUNICODE /D_UNICODE /D__UNREAL__ /DIS_MONOLITHIC=0 /DWITH_ENGINE=1 /DWITH_UNREAL_DEVELOPER_TOOLS=1 /DWITH_UNREAL_TARGET_DEVELOPER_TOOLS=1 /DWITH_APPLICATION_CORE=1 /DWITH_COREUOBJECT=1 /DWITH_VERSE=1 /DUE_USE_VERSE_PATHS=1 /DUSE_STATS_WITHOUT_ENGINE=0 /DWITH_PLUGIN_SUPPORT=0 /DWITH_ACCESSIBILITY=1 /DWITH_PERFCOUNTERS=1 /DUSE_LOGGING_IN_SHIPPING=0 /DWITH_LOGGING_TO_MEMORY=0 /DUSE_CACHE_FREED_OS_ALLOCS=1 /DUSE_CHECKS_IN_SHIPPING=0 /DUSE_UTF8_TCHARS=0 /DUSE_ESTIMATED_UTCNOW=0 /DUE_ALLOW_EXEC_COMMANDS_IN_SHIPPING=1 /DWITH_EDITOR=1 /DWITH_IOSTORE_IN_EDITOR=1 /DWITH_SERVER_CODE=1 /DUE_FNAME_OUTLINE_NUMBER=0 /DWITH_PUSH_MODEL=1 /DWITH_CEF3=1 /DWITH_LIVE_CODING=1 /DWITH_CPP_MODULES=0 /DWITH_CPP_COROUTINES=0 /DWITH_PROCESS_PRIORITY_CONTROL=0 /DUBT_MODULE_MANIFEST=\"UnrealEditor.modules\" /DUBT_MODULE_MANIFEST_DEBUGGAME=\"UnrealEditor-Win64-DebugGame.modules\" /DUBT_COMPILED_PLATFORM=Win64 /DUBT_COMPILED_TARGET=Editor /DUE_APP_NAME=\"UnrealEditor\" /DNDIS_MINIPORT_MAJOR_VERSION=0 /DWIN32=1 /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DPLATFORM_WINDOWS=1 /DPLATFORM_MICROSOFT=1 /DOVERRIDE_PLATFORM_HEADER_NAME=Windows /DRHI_RAYTRACING=1 /DNDEBUG=1 /DUE_BUILD_DEVELOPMENT=1 /DORIGINAL_FILE_NAME=\"UnrealEditor.exe\" /DBUILD_ICON_FILE_NAME=\"..\\Build\\Windows\\Resources\\Default.ico\" /fo \"..\\Intermediate\\Build\\Win64\\UnrealEditor\\Development\\Launch\\PCLaunch.rc.res\" \"Runtime\\Launch\\Resources\\Windows\\PCLaunch.rc\"";
			//string exeArgs = @"@e:\dev\fn\engine\intermediate\build\win64\unrealeditor\development\launch\PCLaunch.rc.res.rsp";
			//RunNormal(exe, exeArgs, workingDir);
			RunBoxed(exe, exeArgs);
		}

		if (batch)
		{
			logger.Info(L"Testing batch");
		
			TString exe = L"C:\\WINDOWS\\system32\\cmd.exe";
			TString exeArgs = L"/C \"call \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\ShaderCompileWorker\\Development\\PostBuild-1.bat\" && type NUL >\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\ShaderCompileWorker\\Development\\PostBuild-1.bat.ran\"\"";

			//TString exe = L"E:\\dev\\fn\\Engine\\Platforms\\Switch\\Build\\BatchFiles\\AuthoringToolHelper.bat";
			//TString exeArgs = L"\"C:\\sdk\\AutoSDK\\HostWin64\\Switch\\15.3.0_4.6.7\\NintendoSDK\\Tools\\CommandLineTools\\AuthoringTool\\AuthoringTool.exe\" \"E:\\dev\\fn\\Engine\\Binaries\\Switch\\UnrealClient.nspd\" \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Switch\\UnrealGame.meta\" \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Switch\\arm64\\UnrealClient\\Development\\UnrealClien_code\" \"E:\\dev\\fn\\Engine\" \"Development\"";

			//string exeArgs = @"@e:\dev\fn\engine\intermediate\build\win64\unrealeditor\development\launch\PCLaunch.rc.res.rsp";
			//RunNormal(exe, exeArgs);
			RunBoxed(exe, exeArgs);
		}

		if (customCasKey)
		{
			TString args = L"@e:\\dev\\fn\\engine\\intermediate\\build\\win64\\x64\\blankprogramnonunity\\development\\core\\SharedPCH.Core.ShadowErrors.h.obj.rsp";
			ProcessHandle h = RunBoxed(win64CompileExe, args, true);
			auto& trackedInputs = h.GetTrackedInputs();
			CasKey key;
			session->GetCasKeyFromTrackedInputs(key, L"", (workingDir + L"\\").c_str(), trackedInputs.data(), u32(trackedInputs.size()));
		}

		if (compileClientServerSocket)
		{
			u32 deleteCount;
			DeleteAllFiles(logger, clientRootDir.c_str(), deleteCount, true);

			if (deleteCas)
				storage->DeleteAllCas();

			server->StartListen(serverBackend, 1456, L"127.0.0.1", cryptoKey);
			auto lg = MakeGuard([server]() { server->StopListen(); });

			u16 port = 1456;

			/*
			bool useProxy = false;
			bool connectDirectlyToProxy = false;
			u32 storagePort = port;

			NetworkClient storageProxyClient;
			NetworkServer storageProxyServer;
			if (useProxy)
			{
				storageProxyClient.Connect(L"127.0.0.1", storagePort);
				storagePort = 1457;
				storageProxyServer.StartListen(storagePort, L"127.0.0.1");
			}
			StorageProxy p(storageProxyServer, storageProxyClient, L"localhost");
			*/

			//while (true)
			{
				#if UBA_USE_QUIC
				NetworkBackendQuic clientBackend(logWriter);
				#else
				NetworkBackendTcp clientBackend(logWriter);
				#endif

				NetworkClientCreateInfo ncci(logWriter);
				ncci.cryptoKey128 = cryptoKey;
				ncci.sendSize = sendSize;
				auto client = new NetworkClient(ctorSuccess, ncci);
				auto destroyClient = MakeGuard([&]() { delete client; });

				NetworkClient* client2 = client;
				NetworkClient* storageClient = nullptr;
				auto destroyClient2 = MakeGuard([&]() { delete storageClient; });
				/*
				if (useProxy)
				{
					storageClient = CreateNetworkClient(logWriter, sendSize);
					storageClient->Connect(L"127.0.0.1", storagePort);
					if (connectDirectlyToProxy)
						client2 = storageClient;
				}
				*/

				struct Proxy
				{
					NetworkClient* client;
					NetworkServer* server = nullptr;
					StorageProxy* storage = nullptr;
				} proxy{ client };
				auto psg = [&]() { delete proxy.server; };
				auto pg = [&]() { delete proxy.storage; };

				static auto startProxy = [](void* userData, u16 proxyPort, const Guid& storageServerUid)
					{
						auto& proxy = *(Proxy*)userData;
						NetworkServerCreateInfo pnsci(g_consoleLogWriter);
						pnsci.workerCount = 16;
						bool ctorSuccess = true;
						proxy.server = new NetworkServer(ctorSuccess, pnsci, L"UbaProxyServer");
						proxy.storage = new StorageProxy(*proxy.server, *proxy.client, storageServerUid, L"Wooohoo");
						return proxy.server->StartListen(proxy.client->GetTcpBackend(), proxyPort);
					};


				StorageClientCreateInfo scci(*client2, clientRootDir.c_str());
				scci.casCapacityBytes = 0;
				scci.storeCompressed = clientStoreCompressed;
				scci.sendCompressed = clientSendCompressed;
				scci.workManager = client2;
				scci.startProxyCallback = startProxy;
				scci.startProxyUserData = &proxy;
				scci.zone = L"FOO";
				auto clientStorage = new StorageClient(scci);
				auto destroyClientStorage = MakeGuard([&]() { delete clientStorage; });

				if (deleteCas)
					clientStorage->DeleteAllCas();
				clientStorage->LoadCasTable();

				SessionClientCreateInfo scInfo(*clientStorage, *client, logWriter);
				scInfo.rootDir = clientRootDir.c_str();
				scInfo.logToFile = true;
				scInfo.deleteSessionsOlderThanSeconds = 1;
				scInfo.maxProcessCount = 32;
				auto clientSession = new SessionClient(scInfo);
				auto destroyClientSession = MakeGuard([&]() { delete clientSession; });


				if (true)
				{
					client->StartListen(clientBackend, 4444);
					server->AddClient(serverBackend, L"127.0.0.1", 4444, cryptoKey);
					uba::Sleep(1000);
					client->SetConnectionCount(4);
				}
				else
				{
					if (!client->Connect(clientBackend, L"127.0.0.1", port))
						return logger.Error(L"Failed to connect");
				}

				if (false)
				{
					session->RegisterCustomService([](const Guid& connectionUid, const void* recv, u32 recvSize, void* send, u32 sendCapacity, void *)
						{
							wprintf(L"GOT MESSAGE: %.*s\n", recvSize / 2, (const wchar_t*)recv);
							const wchar_t* hello = L"Hello response from server";
							u64 helloBytes = wcslen(hello) * 2;
							memcpy(send, hello, helloBytes);
							return u32(helloBytes);
						});

					RunBoxedRemote(L"UbaTestApp.exe", L"");
				}

				//ProcessStartInfo pinfo;
				//pinfo.application = L"PVS-Studio.exe";
				//pinfo.arguments = L" --source-file Module.AudioCaptureCore.cpp --output-file Module.AudioCaptureCore.cpp.i.pvslog --cfg Module.AudioCaptureCore.cpp.i.cfg --i-file=Module.AudioCaptureCore.cpp.i --analysis-mode 4 --lic-file PVS-Studio.lic";
				//pinfo.workingDir = L"e:\\temp\\pvs";
				//pinfo.logLineUserData = &logger;
				//pinfo.logLineFunc = [](void* userData, const wchar_t* line, u32 length) { ((Logger*)userData)->Log(LogEntryType_Info, line, length); };
				//u64 start = GetTime();
				//ProcessHandle process = session->RunProcessRemote(pinfo);
				//process.WaitForExit(INFINITE);
				//u64 time = GetTime() - start;
				//if (process.GetExitCode() != 0)
				//	logger.Error(L"Error exit code: %i", process.GetExitCode());
				//logger.Info(L"Remote run took %ls", TimeToText(time).str);


				//RunBoxedRemote(win64CompileExe, L"@..\\Intermediate\\Build\\Win64\\x64\\BlankProgramNU\\Development\\TraceLog\\SingleFile\\Trace.cpp.rsp");
				for (u32 i = 0; i != 3; ++i)
				{
					RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNU\\Development\\Core\\Stats.cpp.obj.rsp");
					RunBoxedRemote(win64CompileExe, L"@..\\Intermediate\\Build\\Win64\\x64\\BlankProgramNU\\Development\\TraceLog\\Trace.cpp.obj.rsp");
				}
				//winClangCompileExe = L"c:\\sdk\\AutoSDK\\HostWin64\\Win64\\LLVM\\16.0.0\\bin\\clang-cl.exe";
				//RunBoxedRemote(winClangCompileExe, L"@E:\\dev\\fn\\Sandbox\\DevTools\\Clang\\Intermediate\\Build\\Win64\\x64\\ClangGame\\Debug\\AdvancedWidgets\\Module.AdvancedWidgets.cpp.obj.rsp");
				//RunBoxedRemote(winClangCompileExe, L"@E:\\dev\\fn\\Sandbox\\DevTools\\Clang\\Intermediate\\Build\\Win64\\x64\\ClangGame\\Debug\\Core\\MiMalloc.c.obj.rsp");

				//TString rcexe = winSdkDir + L"\\rc.exe";
				//TString rcargs = L"/nologo /D_WIN64 /l 0x409 /I \".\" /I \"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\INCLUDE\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\ucrt\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\shared\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\um\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\winrt\" /DIS_PROGRAM=1 /DUSE_SHADER_COMPILER_WORKER_TRACE=0 /DENABLE_PGO_PROFILE=0 /DUSE_VORBIS_FOR_STREAMING=1 /DUSE_XMA2_FOR_STREAMING=1 /DWITH_DEV_AUTOMATION_TESTS=1 /DWITH_PERF_AUTOMATION_TESTS=1 /DWITH_LOW_LEVEL_TESTS=0 /DWITH_TESTS=1 /DUNICODE /D_UNICODE /D__UNREAL__ /DIS_MONOLITHIC=1 /DWITH_ENGINE=0 /DWITH_UNREAL_DEVELOPER_TOOLS=0 /DWITH_UNREAL_TARGET_DEVELOPER_TOOLS=0 /DWITH_APPLICATION_CORE=0 /DWITH_COREUOBJECT=0 /DWITH_VERSE=1 /DUE_USE_VERSE_PATHS=1 /DUSE_STATS_WITHOUT_ENGINE=0 /DWITH_PLUGIN_SUPPORT=0 /DWITH_ACCESSIBILITY=0 /DWITH_PERFCOUNTERS=0 /DWITH_FIXED_TIME_STEP_SUPPORT=1 /DUSE_LOGGING_IN_SHIPPING=0 /DWITH_LOGGING_TO_MEMORY=0 /DUSE_CACHE_FREED_OS_ALLOCS=1 /DUSE_CHECKS_IN_SHIPPING=0 /DUSE_UTF8_TCHARS=0 /DUSE_ESTIMATED_UTCNOW=0 /DUE_ALLOW_EXEC_COMMANDS_IN_SHIPPING=1 /DWITH_EDITOR=0 /DWITH_SERVER_CODE=1 /DUE_FNAME_OUTLINE_NUMBER=0 /DWITH_PUSH_MODEL=0 /DWITH_CEF3=1 /DWITH_LIVE_CODING=0 /DWITH_CPP_MODULES=0 /DWITH_CPP_COROUTINES=0 /DWITH_PROCESS_PRIORITY_CONTROL=0 /DUBT_MODULE_MANIFEST=\"BlankProgramNonUnity.modules\" /DUBT_MODULE_MANIFEST_DEBUGGAME=\"BlankProgramNonUnity-Win64-DebugGame.modules\" /DUBT_COMPILED_PLATFORM=Win64 /DUBT_COMPILED_TARGET=Program /DUE_APP_NAME=\"BlankProgramNonUnity\" /DUE_ENGINE_DIRECTORY=\"../../\" /DNDIS_MINIPORT_MAJOR_VERSION=0 /DWIN32=1 /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DPLATFORM_WINDOWS=1 /DPLATFORM_MICROSOFT=1 /DOVERRIDE_PLATFORM_HEADER_NAME=Windows /DRHI_RAYTRACING=1 /DNDEBUG=1 /DUE_BUILD_DEVELOPMENT=1 /DORIGINAL_FILE_NAME=\"BlankProgramNonUnity.exe\" /DBUILT_FROM_CHANGELIST=25158763 /DBUILD_VERSION=++Fortnite+Main-CL-25158763 /DBUILD_ICON_FILE_NAME=\"..\\Build\\Windows\\Resources\\Default.ico\" /DPROJECT_COMPANY_NAME=\"Epic Games, Inc.\" /DPROJECT_COPYRIGHT_STRING=\"Copyright Epic Games, Inc. All Rights Reserved.\" /fo \"..\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\Default.rc2.res\" \"..\\Build\\Windows\\Resources\\Default.rc2\"";
				//RunBoxedRemote(rcexe, rcargs);

				//TString linkexe = winSdkDir + L"\\link.exe";
				//TString linkargs = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\BlankProgramNonUnity.exe.rsp";
				//TString linkargs = L"/LIB @e:\\dev\\fn\\engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core\\UnrealPakNU-Core.lib.rsp";
				//RunBoxedRemote(linkexe, linkargs);

				//TString libexe = winSdkDir + L"\\lib.exe";
				//TString libargs = L"@e:\\dev\\fn\\engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core\\UnrealPakNU-Core.lib.rsp";
				//RunBoxedRemote(libexe, libargs);

				//RunBoxedRemote(L"e:\\dev\\fn\\Engine\\Binaries\\Win64\\ShaderCompileWorker.exe", L"\"c:\\Users\\henrik.karlsson\\AppData\\Local\\Temp\\UnrealBoxWorkingDir\\3435EFB64F1EB1CF0D8C7C98D638BC66\\1001\" 2952 0 1000.box.in 0-box.out  -Multiprocess");

				//RunBoxedRemote(win64LinkExe, L"@E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\TraceLog\\UnrealPak-TraceLog.dll.rsp");
				//RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core\\Context.cpp.obj.rsp");
				//RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core\\MiMalloc.c.obj.rsp");
				//RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core\\PCH.Core.h.obj.rsp");//

				//RunBoxedRemote(androidCompileExe, L"@../Intermediate/Build/Android/a/UnrealGame/Development/GoogleOboe/Utilities.cpp.o.rsp");
				//RunBoxed(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\BuildSettings\\BuildSettings.cpp.obj.rsp");

				//RunBoxedRemote(win64LinkExe, L"@E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\TraceLog\\UnrealPakNU-TraceLog.dll.rsp");

				//TString exe = msvcBinDir + L"lib.exe";
				//TString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Json\\UnrealPakNU-Json.lib.rsp";
				//RunBoxedRemote(exe, args);

				//TString args2 = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\Core\\MiMalloc.c.obj.rsp";
				//RunBoxedRemote(win64CompileExe, args2);

				//TString args3 = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNU\\Development\\ImageWrapper\\TiffImageWrapper.cpp.obj.rsp";
				//RunBoxedRemote(win64CompileExe, args3);

				//BoxTraceReader reader(logger);
				//BoxTraceView view;
				//reader.StartReadNamed(view, L"TESTTRACE");

				if (false)
				{
					clientSession->PrintSummary(logger);
					clientStorage->PrintSummary(logger);
					client->PrintSummary(logger);
				}
			}
			server->StopAll();

			if (false)
			{
				session->PrintSummary(logger);
				storage->PrintSummary(logger);
				server->PrintSummary(logger);
				SystemStats::GetGlobal().Print(logger, true);
			}
		}
	}
#endif
}
