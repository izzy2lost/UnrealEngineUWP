// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubRun.h"
#include "Misc/CoreDelegates.h"
#include "LaunchEngineLoop.h"

#ifndef WITH_ASSET_LOADING_AUDIT
#define WITH_ASSET_LOADING_AUDIT 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLiveLinkHub, Log, All);

int32 RunLiveLinkHub(const TCHAR* CommandLine)
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

#if WITH_ASSET_LOADING_AUDIT
	FCoreDelegates::OnSyncLoadPackage.AddLambda([](const FString& PackageName)
		{
			UE_LOG(LogLiveLinkHub, Display, TEXT("Audit: Loaded %s"), *PackageName);
		});
#endif

	// Start up the main loop, adding some extra command line arguments:
	const int32 Result = GEngineLoop.PreInit(*FString::Printf(TEXT("%s %s"), CommandLine, TEXT("LiveLinkHubCommandlet -Messaging -DDC=NoShared -NoShaderCompile")));

	if (Result != 0)
	{
		UE_LOG(LogLiveLinkHub, Error, TEXT("EngineLoop PreInit failed!"));
		return Result;
	}

	GEngineLoop.Exit();

	return Result;
}