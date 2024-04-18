// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealVirtualizationTool.h"

#include "Modules/ModuleManager.h"
#include "ProjectUtilities.h"
#include "RequiredProgramMainCPPInclude.h"
#include "UnrealVirtualizationToolApp.h"

IMPLEMENT_APPLICATION(UnrealVirtualizationTool, "UnrealVirtualizationTool");

DEFINE_LOG_CATEGORY(LogVirtualizationTool);

int32 UnrealVirtualizationToolMain(int32 ArgC, TCHAR* ArgV[])
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UnrealVirtualizationToolMain);

	using namespace UE::Virtualization;

	// Allows this program to accept a project argument on the commandline and use project-specific config
	UE::ProjectUtilities::ParseProjectDirFromCommandline(ArgC, ArgV);

	GEngineLoop.PreInit(ArgC, ArgV);
	check(GConfig && GConfig->IsReadyForUse());

#if 0
	while (!FPlatformMisc::IsDebuggerPresent())
	{
		FPlatformProcess::SleepNoStats(0.0f);
	}

	PLATFORM_BREAK();
#endif

	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	bool bRanSuccessfully = true;

	FUnrealVirtualizationToolApp App;

	EInitResult Result = App.Initialize();
	if (Result == EInitResult::Success)
	{
		if (!App.Run())
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("UnrealVirtualizationTool ran with errors"));
			bRanSuccessfully = false;
		}
	}	
	else if(Result == EInitResult::Error)
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("UnrealVirtualizationTool failed to initialize"));
		bRanSuccessfully = false;
	}

	UE_CLOG(bRanSuccessfully, LogVirtualizationTool, Display, TEXT("UnrealVirtualizationTool ran successfully"));

	const uint8 ReturnCode = bRanSuccessfully ? 0 : 1;

	if (FParse::Param(FCommandLine::Get(), TEXT("fastexit")))
	{
		FPlatformMisc::RequestExitWithStatus(true, ReturnCode);
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Shutdown);

		GConfig->DisableFileOperations(); // We don't want to write out any config file changes!

		// Even though we are exiting anyway we need to request an engine exit in order to get a clean shutdown
		RequestEngineExit(TEXT("The process has finished"));

		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();
	}

	return ReturnCode;
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);
	return UnrealVirtualizationToolMain(ArgC, ArgV);
}