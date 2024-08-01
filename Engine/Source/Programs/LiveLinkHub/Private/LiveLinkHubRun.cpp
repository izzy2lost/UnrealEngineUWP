// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubRun.h"

#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformSplash.h"
#include "ILiveLinkHubModule.h"
#include "Interfaces/IPluginManager.h"
#include "LaunchEngineLoop.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#ifndef WITH_ASSET_LOADING_AUDIT
#define WITH_ASSET_LOADING_AUDIT 0
#endif

#if WITH_ASSET_LOADING_AUDIT
#	include "Misc/CoreDelegates.h"
#endif


DEFINE_LOG_CATEGORY_STATIC(LogLiveLinkHub, Log, All);


// Opt in to new D3D12 redist and tell the loader where to search for D3D12Core.dll.
// The D3D loader looks for these symbol exports in the .exe module.
// We only support this on x64 Windows Desktop platforms. Other platforms or non-redist-aware 
// versions of Windows will transparently load default OS-provided D3D12 library.
#define USE_D3D12_REDIST (PLATFORM_DESKTOP && PLATFORM_CPU_X86_FAMILY && PLATFORM_64BITS && 1)
#if USE_D3D12_REDIST
extern "C" { _declspec(dllexport) extern const UINT D3D12SDKVersion = 614; } // D3D12_SDK_VERSION
extern "C" { _declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }
#endif // USE_D3D12_REDIST



int32 RunLiveLinkHub(const TCHAR* CommandLine)
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

	// Needs to be initialized early for splash / mount points / plugin search paths.
	FCommandLine::Set(CommandLine);

	const FText AppName = NSLOCTEXT("LiveLinkHub", "SplashTextName", "LiveLink Hub");
	FPlatformSplash::SetSplashText(SplashTextType::GameName, *AppName.ToString());

#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("WaitForDebugger")))
	{
		while (!FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformProcess::Sleep(0.1f);
		}
		UE_DEBUG_BREAK();
	}
#endif

#if WITH_ASSET_LOADING_AUDIT
	FCoreDelegates::OnSyncLoadPackage.AddLambda([](const FString& PackageName)
		{
			UE_LOG(LogLiveLinkHub, Display, TEXT("Audit: Loaded %s"), *PackageName);
		});
#endif

#if !IS_PROGRAM
	const TCHAR* const DevelopmentProjectPath = TEXT("../../Source/Programs/LiveLinkHubEditor/LiveLinkHubEditor.uproject");
	const TCHAR* const StagedProjectPath = TEXT("../../../LiveLinkHubEditor/LiveLinkHubEditor.uproject");

	if (FPaths::FileExists(DevelopmentProjectPath))
	{
		FPaths::SetProjectFilePath(DevelopmentProjectPath);
	}
	else if (FPaths::FileExists(StagedProjectPath))
	{
		FPaths::SetProjectFilePath(StagedProjectPath);
	}
#endif

#if IS_PROGRAM
	// Disable this when going through PreInit to prevent the console window from appearing.
	GIsSilent = true;
#endif

	// Start up the main loop, adding some extra command line arguments:
#if !IS_PROGRAM
	int32 Result = GEngineLoop.PreInit(*FString::Printf(TEXT("%s %s"), TEXT(""), CommandLine));
#else
	int32 Result = GEngineLoop.PreInit(*FString::Printf(TEXT("%s %s"), CommandLine, TEXT("-RUN=LiveLinkHubCommandlet -Messaging -DDC=NoShared -NoShaderCompile")));
#endif

	if (Result != 0)
	{
		UE_LOG(LogLiveLinkHub, Error, TEXT("EngineLoop PreInit failed (%i)"), Result);
	}

	if (Result != 0 || IsEngineExitRequested())
	{
		return Result;
	}

#if !IS_PROGRAM
	{
		ILiveLinkHubModule& HubModule = FModuleManager::Get().LoadModuleChecked<ILiveLinkHubModule>("LiveLinkHub");
		HubModule.PreinitializeLiveLinkHub();

		Result = GEngineLoop.Init();

		HubModule.StartLiveLinkHub();

		// Hide the splash screen now that everything is ready to go
		FPlatformSplash::Hide();

		while (!IsEngineExitRequested())
		{
			GEngineLoop.Tick();
		}

		HubModule.ShutdownLiveLinkHub();
	}
#endif

	GEngineLoop.Exit();

	return Result;
}
