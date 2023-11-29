// Copyright Epic Games, Inc. All Rights Reserved.

#define BOX_API

#include <iostream>
// #include "UbaNetworkClient.h"
// #include "UbaNetworkServer.h"
// #include "UbaSessionClient.h"
// #include "UbaSessionServer.h"
// #include "UbaStorageClient.h"
// #include "UbaStorageServer.h"
// #include "UbaProcess.h"
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

#if 0
#include "BoxVisualizer.h"
#include "ComputeChannel.h"
#endif


// using namespace uba;

int main(int argc, char* argv[])
{
	using namespace uba;

	if (!RunAllTests())
		return -1;
}
// int main() {
//     // LogWriter& logWriter = g_consoleLogWriter;
//     // LoggerWithWriter logger(logWriter, L"");
//     // u32 port = 1456;
    
//     // int sendSize = SendDefaultSize;// 64 * 1024;
    
//     // {
//     //     BoxTcpBackend clientBackend(logWriter);
//     //     BoxTcpBackend serverBackend(logWriter);
        
//     //     BoxServer* server = new BoxServer(logWriter, 64, sendSize, 0);
//     //     server->StartListen(serverBackend, port, L"127.0.0.1");
//     //     Sleep(200);
//     //     BoxClient* client = CreateBoxClient(logWriter, sendSize);
//     //     auto destroyClient = MakeGuard([&]() { DestroyBoxClient(client); });

//     //     client->Connect(clientBackend, L"127.0.0.1", port);
//     //     server->PrintSummary(logger);
//     //     client->Disconnect();
//     //     server->StopAll();
//     // }
// }

// //struct NamedMemRouter
// //{
// //    NamedMemRouter(const wchar_t* name)
// //    {
// ////        static const int NumChunks = 2;
// ////        static const int ChunkSize = 32 * 1024;
// ////        channel.CreateNew(name, NumChunks, ChunkSize);
// //    }
// //
// //    ~NamedMemRouter()
// //    {
// //        BOX_ASSERT(!looping);
// //    }
// //
// //    void StartSend(NamedMemRouter& other)
// //    {
// //        sendThread.Start([&]()
// //            {
// ////                FComputeBufferReader reader;
// ////                reader.OpenExisting(other.channel.SendBuffer.GetName(), 0);
// ////
// ////                FComputeBufferWriter writer;
// ////                writer.OpenExisting(channel.RecvBuffer.GetName());
// ////
// ////                while (looping)
// ////                {
// ////                    const unsigned char* readData;
// ////                    while ((readData = reader.WaitToRead(1, 500)) == nullptr)
// ////                        if (!looping)
// ////                            break;
// ////
// ////                    unsigned char* writeData;
// ////                    while ((writeData = writer.WaitToWrite(1, 500)) == nullptr)
// ////                        if (!looping)
// ////                            break;
// ////
// ////                    size_t readSize = reader.GetReadBufferSize();
// ////                    size_t writeSize = writer.GetWriteBufferSize();
// ////
// ////                    size_t copySize = (readSize < writeSize) ? readSize : writeSize;
// ////                    memcpy(writeData, readData, copySize);
// ////
// ////                    reader.AdvanceReadPosition(copySize);
// ////                    writer.AdvanceWritePosition(copySize);
// ////                }
// ////
// ////                writer.MarkComplete();
// //                return 0;
// //            });
// //    }
// //
// //    void Stop()
// //    {
// //        looping = false;
// //        sendThread.Wait();
// //    }
// //
// ////    FComputeChannel channel;
// //
// //    bool looping = true;
// //    Thread sendThread;
// //};
// //
// //int main()
// //{
// //    WString rootDir(L"/tmp/Box");
// //
// //    WString workingDir = L"./";
// //
// //    WString clientRootDir = L"/tmp/BoxClient";
// //    WString serverRootDir = L"/tmp/BoxServer";
// //
// //    WString macSdkDir = L"/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.3.sdk";
// ////    WString winSdkDir = L"c:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22000.0\\x64";
// //    //WString msvcBinDir = L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\bin\\Hostx64\\X64\\";
// //    WString xcodeBinDir = L"/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/";
// //
// //    //WString linuxSysRoot = L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\x64";
// ////    WString linuxSysRoot = L"c:/sdk/AutoSDK/HostWin64/Linux_x64/v21_clang-15.0.1-centos7/x86_64-unknown-linux-gnu";
// //    //WString linuxSysRoot = L"e:\\dev\\fn\\Engine\\Source\\ThirdParty\\IWYU\\llvm1501\\build";
// ////    WString linuxBinDir = linuxSysRoot + L"/bin/";
// //
// ////    WString androidBinDir = L"c:\\sdk\\autosdk\\hostwin64\\android\\-26\\ndk\\25.1.8937393\\toolchains\\llvm\\prebuilt\\windows-x86_64\\bin\\";
// ////    WString androidCompileExe = androidBinDir + L"clang++.exe";
// //
// ////    WString msvcCompileExe = msvcBinDir + L"cl.exe";
// //    WString macClangCompileExe = xcodeBinDir + L"clang";
// ////    WString linuxCompileExe = linuxBinDir + L"clang++.exe";
// //
// ////    WString win64CompileExe = msvcCompileExe;
// //    //WString win64CompileExe = winClangCompileExe;
// //
// ////    WString win64LinkExe = msvcBinDir + L"link.exe";
// //    WString macLinkExe = xcodeBinDir + L"link";
// //
// //    LogWriter& logWriter = g_consoleLogWriter;
// //    LoggerWithWriter logger(logWriter, L"");
// //    bool serverStoreCompressed = true; // Will both store and send compressed if true..
// //    bool clientStoreCompressed = true; // Will store received files compressed on disk (with a few exceptions, which is binaries)
// //    bool clientSendCompressed = true; // Will compress file when sent to server
// //    bool launchVisualizer = false;
// //    bool shouldWriteToDisk = true;
// //
// ////    {
// ////        StringBuffer<64*1024> env;
// ////        env.count = GetEnvironmentVariableW(L"PATH", env.data, 64*1024);
// ////        env.Append(L";").Append(macSdkDir).Append(L";").AppendDir(StringBuffer<>(macClangCompileExe));
// ////        SetEnvironmentVariableW(L"PATH", env.data);
// ////    }
// //
// //    int sendSize = SendDefaultSize;// 64 * 1024;
// //    //for (int sendSize=4224; sendSize!=250000; ++sendSize)
// //    //while (true)
// //    {
// //        if (sendSize != SendDefaultSize)
// //            logger.Info(L"Box message send size: %u", sendSize);
// //        BoxServer* server = CreateBoxServer(logWriter, 64, sendSize);
// //        BoxTcpBackend serverBackend;
// //        auto destroyServer = MakeGuard([&]() { DestroyBoxServer(server); });
// //        
// //        BoxStorageServer* storage = CreateBoxStorageServer(*server, rootDir.c_str(), 0, serverStoreCompressed);
// //        auto destroyStorage = MakeGuard([&]() { DestroyBoxStorage(storage); });
// //        
// //        BoxSessionServerCreateInfo info(*storage, *server);
// ////        info.useUniqueGuid = false;
// //        info.launchVisualizer = launchVisualizer;
// //        info.shouldWriteToDisk = shouldWriteToDisk;
// //        info.rootDir = rootDir.c_str();
// //        info.traceName = L"TESTTRACE";
// //        info.deleteSessionsOlderThanSeconds = 1;
// //        BoxSessionServer* session = CreateBoxSessionServer(info);
// //        auto destroySession = MakeGuard([&]() { DestroyBoxSession(session); });
// //        
// //        auto RunBoxed = [&](const WString& app, const WString& arg, bool trackInputs = false) -> BoxProcessHandle
// //        {
// //            u64 start = GetTime();
// //            BoxProcessStartInfo pinfo;
// //            pinfo.application = app.c_str();
// //            pinfo.arguments = arg.c_str();
// //            pinfo.workingDir = workingDir.c_str();
// ////            pinfo.logFile = rootDir + L"\\DebugLog.log";
// ////            pinfo.logFile = logFile.c_str();
// ////            pinfo.onLogLine = [&](const wchar_t* line) { logger.Info(line); };
// //            pinfo.trackInputs = trackInputs;
// //            BoxProcessHandle process = session->RunProcess(pinfo, false);
// //            if (process.GetExitCode() != 0)
// //                logger.Error(L"Error exit code: %i", process.GetExitCode());
// //            u64 time = GetTime() - start;
// //            logger.Info(L"Boxed run took %ls", TimeToText(time));
// //            return process;
// //        };
// //        
// //        auto RunNormal = [&](const WString& app, const WString& arg)
// //        {
// //            u64 start = GetTime();
// //            STARTUPINFOW si;
// //            memset(&si, 0, sizeof(si));
// //            PROCESS_INFORMATION pi;
// //            WString cmdLine = app + L" " + arg;
// //            CreateProcessW(NULL, (wchar_t*)cmdLine.c_str(), NULL, NULL, false, 0, NULL, workingDir.c_str(), &si, &pi);
// //            CloseHandle(pi.hThread);
// //            WaitForSingleObject(pi.hProcess, INFINITE);
// //            DWORD exitCode;
// //            GetExitCodeProcess(pi.hProcess, &exitCode);
// //            if (exitCode != 0)
// //                logger.Error(L"Error exit code: %i", exitCode);
// //            CloseHandle(pi.hProcess);
// //            u64 time = GetTime() - start;
// //            logger.Info(L"Normal run took %ls", TimeToText(time));
// //        };(void)RunNormal;
// //        
// //        auto RunBoxedRemote = [&](const WString& app, const WString& arg)
// //        {
// //            BoxProcessStartInfo pinfo;
// //            pinfo.application = app.c_str();
// //            pinfo.arguments = arg.c_str();
// //            pinfo.workingDir = workingDir.c_str();
// ////            pinfo.logFile = rootDir + L"\\DebugLog.log";
// ////            pinfo.logFile = logFile.c_str();
// ////            pinfo.onLogLine = [&](const wchar_t* line) { logger.Info(line); };
// //            BoxProcessHandle process = session->RunProcessRemote(pinfo);
// //            process.WaitForExit(INFINITE);
// //            if (process.GetExitCode() != 0)
// //                logger.Error(L"Error exit code: %i", process.GetExitCode());
// //        };(void)RunBoxedRemote;
// //        
// //        bool compileWin64 = false;
// //        bool compileLinux = false;
// //        bool linkWin64 = false;
// //        bool libWin64 = false;
// //        bool linkLinux = false;
// //        bool copy = false;
// //        bool xcopy = false;
// //        bool writeMeta = false;
// //        bool rc = false;
// //        bool batch = false;
// //        bool customCasKey = false;
// //        bool compileClientServerSocket = false;
// //        bool compileClientServerNamedEvents = false;
// //        bool deleteCas = false;
// //        
// //        //RunBoxed(L"e:\\dev\\fn\\Engine\\Platforms\\XboxCommon\\Binaries\\Win64\\XboxPDBFileUtil.exe", L"e:\\dev\\fn\\FortniteGame\\Binaries\\XSX\\FortniteClient.exe e:\\dev\\fn\\FortniteGame\\Build\\XSX\\Symbols\\FortniteClient-symbols.bin");
// //        
// //        //RunBoxed(L"e:\\dev\\fn\\Engine\\Binaries\\Win64\\UnrealEditor.exe", L"FortniteGame -DisablePython");
// //        
// //        //RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.36.32532\\bin\\Hostx64\\x64\\pgomgr.exe", L"/merge /nologo \"E:\\dev\\fn\\FortniteGame\\Binaries\\Win64\\FortniteClient-Win64-Shipping.pgd");
// //        //RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.36.32532\\bin\\Hostx64\\x64\\link.exe", L"@\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgram\\Shipping\\BlankProgram-Win64-Shipping.exe.rsp\"");
// //        //RunBoxed(L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.36.32532\\bin\\Hostx64\\x64\\link.exe", L"@\"E:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\FortniteClient\\Shipping\\FortniteClient-Win64-Shipping.exe.rsp\"");
// //        
// //        
// //        if (compileWin64)
// //        {
// //            logger.Info(L"Testing compile msvc");
// //            //WString args = L"\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\VisualStudioDTE\\dte80a.cpp\" /c /nologo /Fo\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\VisualStudioDTE\\dte80a.obj\" /I . /external:W0 /external:I \"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\INCLUDE\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\ucrt\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\shared\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\um\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\winrt\"";
// //            //WString args = L"\"e:\\dev\\fn\\Engine\\Plugins\\Runtime\\Database\\ADOSupport\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ADOSupport\\msado15.cpp\" /c /nologo /Fo\"e:\\dev\\fn\\Engine\\Plugins\\Runtime\\Database\\ADOSupport\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\ADOSupport\\msado15.obj\" /I . /external:W0 /external:I \"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\INCLUDE\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\ucrt\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\shared\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\um\" /external:I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\winrt\"";
// //            //WString args = L"@e:\\dev\\fn\\engine\\intermediate\\build\\win64\\x64\\blankprogramnonunity\\development\\core\\SharedPCH.Core.ShadowErrors.h.obj.rsp";
// //            //WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core\\PCH.Core.h.obj.rsp";
// //            //WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\ApplicationCore\\WindowsUIAManager.cpp.obj.rsp";
// //            //WString args = L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\FortniteGame\\PCH.FortniteGame.h.obj.rsp";
// //            WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core\\MiMalloc.c.obj.rsp";
// //            //WString args = L"@e:\\dev\\fn\\Engine\\Plugins\\Runtime\\AR\\Google\\GoogleARCore\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\GoogleARCoreBase\\GoogleARCoreDependencyHandler.cpp.obj.rsp";
// //            //WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Json\\JsonModule.cpp.obj.rsp";
// //            //WString args = L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\FortniteGame\\Module.FortniteGame.127_of_150.cpp.obj.rsp";
// //            //WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\BuildSettings\\BuildSettings.cpp.obj.rsp";
// //            //WString args = L"@..\\Plugins\\Experimental\\NNE\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\NNEORT\\noop_elimination.cc.obj.rsp";
// //            //WString args = L"@e:\\dev\\fn\\engine\\Intermediate\\Build\\Win64\\x64\\BlankProgram\\Development\\Core\\Module.Core.1_of_17.cpp.obj.rsp";
// //            //RunNormal(win64CompileExe, args);
// //#if _MSC_VER
// //            RunBoxed(win64CompileExe, args);
// //#else
// //            assert(1==2);
// //#endif
// //            //RunBoxed(win64CompileExe, args);
// //            //RunBoxed(win64CompileExe, args);
// //            //RunBoxed(win64CompileExe, args);
// //            
// //            //for (int i=0;i!=10; ++i)
// //            //    RunBoxed(win64CompileExe, args);
// //            
// //            //session->RefreshDirectory(L"e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core");
// //            
// //            //RunBoxed(win64CompileExe, args);
// //            
// //            //WString args2 = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\BuildSettings\\UnrealPakNonUnity-BuildSettings.dll.rsp";
// //            //RunBoxed(msvcBinDir + L"link.exe", args2);
// //        }
// //        
// //        if (compileLinux)
// //        {
// //            logger.Info(L"Testing compile linux (clang)");
// //            //WString args = L"@e:\\dev\\fn\\engine\\Intermediate\\Build\\Linux\\x64\\BlankProgram\\Development\\Core\\PCH.Core.h.gch.rsp";
// //            WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\BlankProgramNonUnity\\Development\\Core\\ABTesting.cpp.o.rsp";
// //            //WString args = L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Linux\\x64\\UnrealEditor\\Development\\FortniteUI\\Module.FortniteUI.12_of_45.cpp.o.rsp";
// //            //WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\UnrealPakNonUnity\\Development\\ColorManagement\\TransferFunctions.cpp.o.rsp";
// //            //WString args = L"@../Intermediate/Build/Linux/x64/UnrealPakNonUnity/Development/CoreUObject/SharedPCH.CoreUObject.ShadowErrors.h.gch.rsp";
// //            
// //            //for (int i=0;i!=8;++i)
// //            {
// //                //RunNormal(linuxCompileExe, args);
// //#if _MSC_VER
// //                RunBoxed(linuxCompileExe, args);
// //                
// //            }
// //        }
// //        
// //#else
// //        assert(1==2);
// //    }
// //#endif
// //        //if (true)
// //        //{
// //        //    WString args = L"-Xiwyu --mapping_file=E:\\dev\\fn\\Engine\\Binaries\\ThirdParty\\IWYU\\ue_mapping.imp -Xiwyu --prefix_header_includes=keep -Xiwyu --no_check_matching_header -Xiwyu --cxx17ns -Xiwyu --write_json_path=\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\iwyu\\BlankProgramNonUnity\\Development\\Core\\LowLevelTestAdapter.h.iwyu\" @\"E:/dev/fn/Engine/Intermediate/Build/Linux/iwyu/BlankProgramNonUnity/Development/Core/LowLevelTestAdapter.h.iwyu.rsp\"";
// //        //    RunBoxed(L"E:\\dev\\fn\\Engine\\Binaries\\ThirdParty\\IWYU\\include-what-you-use.exe", args);
// //        //}
// //
// //        //if (true)
// //        //{
// //        //    WString exe = L"C:\\sdk\\AutoSDK\\HostWin64\\Switch\\15.3.0_4.6.7\\NintendoSDK\\Compilers\\NX\\nx\\aarch64\\bin\\ld.lld.exe";
// //        //    WString args = L"@e:\\temp\\box\\sessions\\230430_055933\\temp\\response-95b56d.txt";
// //        //    RunBoxed(exe, args);
// //        //}
// //
// //        if (linkWin64)
// //        {
// //            logger.Info(L"Testing link msvc");
// //#if _MSC_VER
// //            WString exe = win64LinkExe;
// //
// //            //WString args = L"@E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\TraceLog\\UnrealPakNonUnity-TraceLog.dll.rsp";
// //            WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\BlankProgramNonUnity.exe.rsp";
// //            RunBoxed(exe, args);
// //#else
// //            assert(1==2);
// //#endif
// //        }
// //
// //        if (libWin64)
// //        {
// //            logger.Info(L"Testing link msvc");
// //#if _MSC_VER
// //            WString exe = msvcBinDir + L"lib.exe";
// //
// //            WString args = L"@E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\NotForLicensees\\uLangCore\\UnrealEditor-uLangCore.lib.rsp";
// //            RunBoxed(exe, args);
// //#else
// //            assert(1==2);
// //#endif
// //        }
// //
// //        if (linkLinux)
// //        {
// //            WString exe = L"C:\\WINDOWS\\system32\\cmd.exe";
// //            //WString exeArgs = L"/C E:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\UnrealPakNonUnity\\Development\\Link-libUnrealPakNonUnity-TraceLog.so.link.bat";
// //            //WString exeArgs = L"/C e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\BlankProgram\\Development\\Link-BlankProgram.link.bat";
// //            //WString exeArgs = L"/C e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\UnrealEditor\\Development\\Link-libUnrealEditor-UMGEditor.so.link.bat";
// //            WString exeArgs = L"/C e:\\dev\\fn\\Engine\\Intermediate\\Build\\Linux\\x64\\UnrealEditor\\Development\\Link-libUnrealEditor-TranslationEditor.so.link.bat";
// //            RunBoxed(exe, exeArgs);
// //        }
// //
// //        if (copy)
// //        {
// //            logger.Info(L"Testing copy");
// //            WString exe = L"C:\\WINDOWS\\system32\\cmd.exe";
// //            WString exeArgs = L"/C \"copy /Y \"E:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Linux\\x64\\FortniteClient\\Development\\Chaos\\PBDEvolution.ispc.generated.dummy.h\" \"E:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Linux\\x64\\FortniteClient\\Development\\Chaos\\PBDEvolution.ispc.generated.h\" 1>nul\"";
// //            //WString exeArgs = L"/C \"copy /Y \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Chaos\\PBDMinEvolution.ispc.generated.dummy.h\" \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\Chaos\\PBDMinEvolution.ispc.generated.h\" 1>nul\"";
// //            //RunNormal(exe, exeArgs, workingDir);
// //            RunBoxed(exe, exeArgs);
// //        }
// //
// //        if (xcopy)
// //        {
// //            logger.Info(L"Testing xcopy");
// //            WString exe = L"C:\\Windows\\system32\\xcopy.exe";
// //            WString args = L"/s E:\\dev\\fn\\Engine\\Intermediate\\Build\\Switch\\HtmlDocument_Staging\\Development\\*.* \"E:\\dev\\fn\\Engine\\Binaries\\Switch\\UnrealClient.nspd\\htmlDocument0.ncd\\data\\html-document\"";
// //            RunBoxed(exe, args);
// //        }
// //
// //        if (writeMeta)
// //        {
// //            logger.Info(L"Testing writemeta");
// //            //WString exeArgs1 = L"\"E:\\dev\\fn\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.dll\" -Mode=WriteMetadata -Input=E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\UnrealPakNonUnity\\Development\\EngineMetadata.dat -Version=2";
// //            //WString exeArgs2 = L"\"E:\\dev\\fn\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.dll\" -Mode=WriteMetadata -Input=E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\UnrealPakNonUnity\\Development\\TargetMetadata.dat -Version=2";
// //            WString exeArgs1 = L"E:\\dev\\fn\\Engine\\Binaries\\DotNET\\UnrealBuildTool\\UnrealBuildTool.dll -Mode=WriteMetadata -Input=e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\TargetMetadata.dat -Version=2";
// //            WString exe = L"E:\\dev\\fn\\Engine\\Binaries\\ThirdParty\\DotNet\\6.0.302\\windows\\dotnet.exe";
// //            RunBoxed(exe, exeArgs1);
// //            //RunBoxed(exe, exeArgs2);
// //        }
// //
// //        if (rc)
// //        {
// //            logger.Info(L"Testing rc");
// //#if _MSC_VER
// //            WString exe = winSdkDir + L"\\rc.exe";
// //            WString exeArgs = L"/nologo /D_WIN64 /l 0x409 /I \".\" /I \"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\INCLUDE\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\ucrt\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\shared\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\um\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\winrt\" /DIS_PROGRAM=0 /DUE_EDITOR=1 /DENABLE_PGO_PROFILE=0 /DUSE_VORBIS_FOR_STREAMING=1 /DUSE_XMA2_FOR_STREAMING=1 /DWITH_DEV_AUTOMATION_TESTS=1 /DWITH_PERF_AUTOMATION_TESTS=1 /DWITH_LOW_LEVEL_TESTS=0 /DWITH_TESTS=1 /DUNICODE /D_UNICODE /D__UNREAL__ /DIS_MONOLITHIC=0 /DWITH_ENGINE=1 /DWITH_UNREAL_DEVELOPER_TOOLS=1 /DWITH_UNREAL_TARGET_DEVELOPER_TOOLS=1 /DWITH_APPLICATION_CORE=1 /DWITH_COREUOBJECT=1 /DWITH_VERSE=1 /DUE_USE_VERSE_PATHS=1 /DUSE_STATS_WITHOUT_ENGINE=0 /DWITH_PLUGIN_SUPPORT=0 /DWITH_ACCESSIBILITY=1 /DWITH_PERFCOUNTERS=1 /DUSE_LOGGING_IN_SHIPPING=0 /DWITH_LOGGING_TO_MEMORY=0 /DUSE_CACHE_FREED_OS_ALLOCS=1 /DUSE_CHECKS_IN_SHIPPING=0 /DUSE_UTF8_TCHARS=0 /DUSE_ESTIMATED_UTCNOW=0 /DUE_ALLOW_EXEC_COMMANDS_IN_SHIPPING=1 /DWITH_EDITOR=1 /DWITH_IOSTORE_IN_EDITOR=1 /DWITH_SERVER_CODE=1 /DUE_FNAME_OUTLINE_NUMBER=0 /DWITH_PUSH_MODEL=1 /DWITH_CEF3=1 /DWITH_LIVE_CODING=1 /DWITH_CPP_MODULES=0 /DWITH_CPP_COROUTINES=0 /DWITH_PROCESS_PRIORITY_CONTROL=0 /DUBT_MODULE_MANIFEST=\"UnrealEditor.modules\" /DUBT_MODULE_MANIFEST_DEBUGGAME=\"UnrealEditor-Win64-DebugGame.modules\" /DUBT_COMPILED_PLATFORM=Win64 /DUBT_COMPILED_TARGET=Editor /DUE_APP_NAME=\"UnrealEditor\" /DNDIS_MINIPORT_MAJOR_VERSION=0 /DWIN32=1 /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DPLATFORM_WINDOWS=1 /DPLATFORM_MICROSOFT=1 /DOVERRIDE_PLATFORM_HEADER_NAME=Windows /DRHI_RAYTRACING=1 /DNDEBUG=1 /DUE_BUILD_DEVELOPMENT=1 /DORIGINAL_FILE_NAME=\"UnrealEditor.exe\" /DBUILD_ICON_FILE_NAME=\"..\\Build\\Windows\\Resources\\Default.ico\" /fo \"..\\Intermediate\\Build\\Win64\\UnrealEditor\\Development\\Launch\\PCLaunch.rc.res\" \"Runtime\\Launch\\Resources\\Windows\\PCLaunch.rc\"";
// //            //string exeArgs = @"@e:\dev\fn\engine\intermediate\build\win64\unrealeditor\development\launch\PCLaunch.rc.res.rsp";
// //            //RunNormal(exe, exeArgs, workingDir);
// //            RunBoxed(exe, exeArgs);
// //#else
// //            assert(1==2);
// //#endif
// //        }
// //
// //        if (batch)
// //        {
// //            logger.Info(L"Testing batch");
// //        
// //            WString exe = L"C:\\WINDOWS\\system32\\cmd.exe";
// //            WString exeArgs = L"/C \"call \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\ShaderCompileWorker\\Development\\PostBuild-1.bat\" && type NUL >\"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\ShaderCompileWorker\\Development\\PostBuild-1.bat.ran\"\"";
// //
// //            //WString exe = L"E:\\dev\\fn\\Engine\\Platforms\\Switch\\Build\\BatchFiles\\AuthoringToolHelper.bat";
// //            //WString exeArgs = L"\"C:\\sdk\\AutoSDK\\HostWin64\\Switch\\15.3.0_4.6.7\\NintendoSDK\\Tools\\CommandLineTools\\AuthoringTool\\AuthoringTool.exe\" \"E:\\dev\\fn\\Engine\\Binaries\\Switch\\UnrealClient.nspd\" \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Switch\\UnrealGame.meta\" \"E:\\dev\\fn\\Engine\\Intermediate\\Build\\Switch\\arm64\\UnrealClient\\Development\\UnrealClien_code\" \"E:\\dev\\fn\\Engine\" \"Development\"";
// //
// //            //string exeArgs = @"@e:\dev\fn\engine\intermediate\build\win64\unrealeditor\development\launch\PCLaunch.rc.res.rsp";
// //            //RunNormal(exe, exeArgs);
// //            RunBoxed(exe, exeArgs);
// //        }
// //
// //        if (customCasKey)
// //        {
// //            WString args = L"@e:\\dev\\fn\\engine\\intermediate\\build\\win64\\x64\\blankprogramnonunity\\development\\core\\SharedPCH.Core.ShadowErrors.h.obj.rsp";
// //#if _MSC_VER
// //            BoxProcessHandle h = RunBoxed(win64CompileExe, args, true);
// //
// //            auto& trackedInputs = h.GetTrackedInputs();
// //            CasKey key;
// //            session->GetCasKeyFromTrackedInputs(key, L"", (workingDir + L"\\").c_str(), trackedInputs.data(), u32(trackedInputs.size()));
// //#else
// //            assert(1==2);
// //#endif
// //        }
// //
// //        if (compileClientServerSocket)
// //        {
// //            WString clientRootDir = L"e:\\temp\\BoxClient";
// //            u32 deleteCount;
// //            DeleteAllFiles(logger, clientRootDir.c_str(), deleteCount, true);
// //
// //            if (deleteCas)
// //                storage->DeleteAllCas();
// //
// //            BoxClient* client = CreateBoxClient(logWriter, sendSize);
// //            auto destroyClient = MakeGuard([&]() { DestroyBoxClient(client); });
// //
// //            BoxStorageClient* clientStorage = CreateBoxStorageClient(*client, clientRootDir.c_str(), 0, clientStoreCompressed, clientSendCompressed);
// //            auto destroyStorage = MakeGuard([&]() { DestroyBoxStorage(clientStorage); });
// //
// //            if (deleteCas)
// //                clientStorage->DeleteAllCas();
// //            clientStorage->LoadCasTable();
// //
// //            BoxSessionClientCreateInfo info(*clientStorage, *client, logWriter);
// ////            info.rootDir.Append(clientRootDir);
// //            info.logToFile = true;
// //            info.deleteSessionsOlderThanSeconds = 1;
// //            BoxSessionClient* clientSession = CreateBoxSessionClient(info);
// //            auto destroyClientSession = MakeGuard([&]() { DestroyBoxSession(clientSession); });
// //
// ////            server->StartListen(1456, L"127.0.0.1");
// //            server->StartListen(serverBackend, 1456, L"127.0.0.1");
// //            u32 port = 1456;
// //
// ////            client->Connect(L"127.0.0.1", 1456);
// //
// //            //WString rcexe = winSdkDir + L"\\rc.exe";
// //            //WString rcargs = L"/nologo /D_WIN64 /l 0x409 /I \".\" /I \"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\MSVC\\14.35.32215\\INCLUDE\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\ucrt\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\shared\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\um\" /I \"C:\\Program Files (x86)\\Windows Kits\\10\\include\\10.0.22000.0\\winrt\" /DIS_PROGRAM=1 /DUSE_SHADER_COMPILER_WORKER_TRACE=0 /DENABLE_PGO_PROFILE=0 /DUSE_VORBIS_FOR_STREAMING=1 /DUSE_XMA2_FOR_STREAMING=1 /DWITH_DEV_AUTOMATION_TESTS=1 /DWITH_PERF_AUTOMATION_TESTS=1 /DWITH_LOW_LEVEL_TESTS=0 /DWITH_TESTS=1 /DUNICODE /D_UNICODE /D__UNREAL__ /DIS_MONOLITHIC=1 /DWITH_ENGINE=0 /DWITH_UNREAL_DEVELOPER_TOOLS=0 /DWITH_UNREAL_TARGET_DEVELOPER_TOOLS=0 /DWITH_APPLICATION_CORE=0 /DWITH_COREUOBJECT=0 /DWITH_VERSE=1 /DUE_USE_VERSE_PATHS=1 /DUSE_STATS_WITHOUT_ENGINE=0 /DWITH_PLUGIN_SUPPORT=0 /DWITH_ACCESSIBILITY=0 /DWITH_PERFCOUNTERS=0 /DWITH_FIXED_TIME_STEP_SUPPORT=1 /DUSE_LOGGING_IN_SHIPPING=0 /DWITH_LOGGING_TO_MEMORY=0 /DUSE_CACHE_FREED_OS_ALLOCS=1 /DUSE_CHECKS_IN_SHIPPING=0 /DUSE_UTF8_TCHARS=0 /DUSE_ESTIMATED_UTCNOW=0 /DUE_ALLOW_EXEC_COMMANDS_IN_SHIPPING=1 /DWITH_EDITOR=0 /DWITH_SERVER_CODE=1 /DUE_FNAME_OUTLINE_NUMBER=0 /DWITH_PUSH_MODEL=0 /DWITH_CEF3=1 /DWITH_LIVE_CODING=0 /DWITH_CPP_MODULES=0 /DWITH_CPP_COROUTINES=0 /DWITH_PROCESS_PRIORITY_CONTROL=0 /DUBT_MODULE_MANIFEST=\"BlankProgramNonUnity.modules\" /DUBT_MODULE_MANIFEST_DEBUGGAME=\"BlankProgramNonUnity-Win64-DebugGame.modules\" /DUBT_COMPILED_PLATFORM=Win64 /DUBT_COMPILED_TARGET=Program /DUE_APP_NAME=\"BlankProgramNonUnity\" /DUE_ENGINE_DIRECTORY=\"../../\" /DNDIS_MINIPORT_MAJOR_VERSION=0 /DWIN32=1 /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 /DPLATFORM_WINDOWS=1 /DPLATFORM_MICROSOFT=1 /DOVERRIDE_PLATFORM_HEADER_NAME=Windows /DRHI_RAYTRACING=1 /DNDEBUG=1 /DUE_BUILD_DEVELOPMENT=1 /DORIGINAL_FILE_NAME=\"BlankProgramNonUnity.exe\" /DBUILT_FROM_CHANGELIST=25158763 /DBUILD_VERSION=++Fortnite+Main-CL-25158763 /DBUILD_ICON_FILE_NAME=\"..\\Build\\Windows\\Resources\\Default.ico\" /DPROJECT_COMPANY_NAME=\"Epic Games, Inc.\" /DPROJECT_COPYRIGHT_STRING=\"Copyright Epic Games, Inc. All Rights Reserved.\" /fo \"..\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\Default.rc2.res\" \"..\\Build\\Windows\\Resources\\Default.rc2\"";
// //            //RunBoxedRemote(rcexe, rcargs);
// //
// //            //WString linkexe = winSdkDir + L"\\link.exe";
// //            //WString linkargs = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\BlankProgramNonUnity.exe.rsp";
// //            //WString linkargs = L"/LIB @e:\\dev\\fn\\engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core\\UnrealPakNonUnity-Core.lib.rsp";
// //            //RunBoxedRemote(linkexe, linkargs);
// //
// //            //WString libexe = winSdkDir + L"\\lib.exe";
// //            //WString libargs = L"@e:\\dev\\fn\\engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core\\UnrealPakNonUnity-Core.lib.rsp";
// //            //RunBoxedRemote(libexe, libargs);
// //#if _MSC_VER
// //            RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\Core\\MiMalloc.c.obj.rsp");
// //#else
// //            assert(1==2);
// //#endif
// //            //RunBoxedRemote(L"e:\\dev\\fn\\Engine\\Binaries\\Win64\\ShaderCompileWorker.exe", L"\"c:\\Users\\henrik.karlsson\\AppData\\Local\\Temp\\UnrealBoxWorkingDir\\3435EFB64F1EB1CF0D8C7C98D638BC66\\1001\" 2952 0 1000.box.in 0-box.out  -Multiprocess");
// //
// //            //RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\BlankProgramNonUnity\\Development\\Core\\MiMalloc.c.obj.rsp");
// //            //RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\FortniteGame\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\FortniteGame\\Module.FortniteGame.1_of_145.cpp.obj.rsp");
// //            //RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core\\PCH.Core.h.obj.rsp");
// //            //RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Slate\\TabManager.cpp.obj.rsp");
// //            //RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\NotForLicensees\\uLangParser\\Module.uLangParser.cpp.obj.rsp");
// //            //RunBoxedRemote(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Plugins\\Messaging\\MessagingDebugger\\Intermediate\\Build\\Win64\\x64\\UnrealEditor\\Development\\MessagingDebugger\\Module.MessagingDebugger.cpp.obj.rsp");
// //
// //            //RunBoxedRemote(androidCompileExe, L"@../Intermediate/Build/Android/a/UnrealGame/Development/GoogleOboe/Utilities.cpp.o.rsp");
// //            //RunBoxed(win64CompileExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\BuildSettings\\BuildSettings.cpp.obj.rsp");
// //            //RunBoxedRemote(win64LinkExe, L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\BuildSettings\\UnrealPakNonUnity-BuildSettings.dll.rsp");
// //
// //            //RunBoxedRemote(win64LinkExe, L"@E:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\TraceLog\\UnrealPakNonUnity-TraceLog.dll.rsp");
// //
// //            //WString exe = msvcBinDir + L"lib.exe";
// //            //WString args = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Json\\UnrealPakNonUnity-Json.lib.rsp";
// //            //RunBoxedRemote(exe, args);
// //
// //            //WString args2 = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core\\MiMalloc.c.obj.rsp";
// //            //RunBoxedRemote(win64CompileExe, args2);
// //
// //            //WString args3 = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\ImageWrapper\\TiffImageWrapper.cpp.obj.rsp";
// //            //RunBoxedRemote(win64CompileExe, args3);
// //
// //            server->StopAll();
// //
// ////            clientSession->PrintSummary();
// ////            clientStorage->PrintSummary();
// ////            client->PrintSummary();
// //        }
// //
// //        if (compileClientServerNamedEvents)
// //        {
// //            NamedMemRouter serverMem(L"Server");
// //            NamedMemRouter clientMem(L"Client");
// //            serverMem.StartSend(clientMem);
// //            clientMem.StartSend(serverMem);
// //
// //            server->AddNamedConnection(L"Server");
// //
// //            BoxClient* client = CreateBoxClient();
// //            auto destroyClient = MakeGuard([&]() { DestroyBoxClient(client); });
// //            BoxStorageClient* clientStorage = CreateBoxStorageClient(*client, clientRootDir.c_str(), 0, clientStoreCompressed, clientSendCompressed);
// //            auto destroyStorage = MakeGuard([&]() { DestroyBoxStorage(clientStorage); });
// //
// //            if (deleteCas)
// //                clientStorage->DeleteAllCas();
// //            clientStorage->LoadCasTable();
// //
// //            BoxSessionClientCreateInfo info(*clientStorage, *client);
// ////            info.rootDir.Append(clientRootDir);
// //            info.logToFile = true;
// //            info.deleteSessionsOlderThanSeconds = 1;
// //            BoxSessionClient* clientSession = CreateBoxSessionClient(info);
// //            auto destroyClientSession = MakeGuard([&]() { DestroyBoxSession(clientSession); });
// //
// //            if (true)
// //                client->ConnectNamed(L"Client");
// //
// //            WString args1 = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core\\MiMalloc.c.obj.rsp";
// //            //WString args1 = L"@e:\\dev\\fn\\Engine\\Intermediate\\Build\\Win64\\x64\\UnrealPakNonUnity\\Development\\Core\\PCH.Core.h.obj.rsp";
// //#if _MSC_VER
// //            RunBoxedRemote(win64CompileExe, args1);
// //#else
// //            assert(1==2);
// //#endif
// //
// //            server->StopAll();
// //            client->Disconnect();
// //
// ////            clientSession->PrintSummary();
// ////            clientStorage->PrintSummary();
// ////            client->PrintSummary();
// //
// //            clientMem.Stop();
// //            serverMem.Stop();
// //        }
// //
// ////        session->PrintSummary();
// ////        storage->PrintSummary();
// ////        server->PrintSummary();
// //    }
// //    
// //    return 0;
// //}
// //}
