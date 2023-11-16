// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubRun.h"

#include "LaunchEngineLoop.h"

DEFINE_LOG_CATEGORY_STATIC(LogLiveLinkHub, Log, All);

int32 RunLiveLinkHub(const TCHAR* CommandLine)
{
	FTaskTagScope Scope(ETaskTag::EGameThread);
	
	// Start up the main loop, adding some extra command line arguments:
	const int32 Result = GEngineLoop.PreInit(*FString::Printf(TEXT("%s %s"), CommandLine, TEXT("LiveLinkHubCommandlet -Messaging")));

	if (Result != 0)
	{
		UE_LOG(LogLiveLinkHub, Error, TEXT("EngineLoop PreInit failed!"));
		return Result;
	}

	GEngineLoop.Exit();

	return Result;
}