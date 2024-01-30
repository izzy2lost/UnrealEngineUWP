// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncAvaBridge.h"

#include "Features/IModularFeatures.h"
#include "IAvaMediaModule.h"
#include "IStormSyncTransportClientModule.h"
#include "IStormSyncTransportServerModule.h"
#include "Misc/CoreDelegates.h"
#include "ModularFeature/StormSyncAvaSyncProvider.h"
#include "Playback/AvaMediaPlaybackServer.h"
#include "Playback/IAvaMediaPlaybackClient.h"
#include "StormSyncAvaBridgeCommon.h"
#include "StormSyncAvaBridgeLog.h"
#include "StormSyncAvaBridgeUtils.h"
#include "StormSyncCoreDelegates.h"

#define LOCTEXT_NAMESPACE "FStormSyncAvaBridgeModule"

void FStormSyncAvaBridgeModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FStormSyncAvaBridgeModule::OnPostEngineInit);
	FStormSyncCoreDelegates::OnStormSyncServerStarted.AddRaw(this, &FStormSyncAvaBridgeModule::OnStormSyncServerStarted);
	FStormSyncCoreDelegates::OnStormSyncServerStopped.AddRaw(this, &FStormSyncAvaBridgeModule::OnStormSyncServerStopped);

	// Instantiate the sync provider feature on module start
	SyncProvider = MakeUnique<FStormSyncAvaSyncProvider>();

	RegisterConsoleCommands();
}

void FStormSyncAvaBridgeModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	FStormSyncCoreDelegates::OnStormSyncServerStarted.RemoveAll(this);
	FStormSyncCoreDelegates::OnStormSyncServerStopped.RemoveAll(this);

	UnregisterConsoleCommands();

	// Unregister the interceptor feature on module shutdown
	if (SyncProvider.IsValid())
	{
		IModularFeatures::Get().UnregisterModularFeature(IAvaMediaSyncProvider::GetModularFeatureName(), SyncProvider.Get());
	}
}

void FStormSyncAvaBridgeModule::OnPostEngineInit()
{
	if (IAvaMediaModule::IsModuleLoaded())
	{
		if (IAvaMediaModule::Get().IsMediaPlaybackServerStarted())
		{
			RegisterUserDataForPlaybackServer();
		}

		if (IAvaMediaModule::Get().IsMediaPlaybackClientStarted())
		{
			RegisterUserDataForPlaybackClient();
		}

		IAvaMediaModule::Get().GetOnAvaMediaPlaybackServerStarted().AddStatic(&FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackServer);
		IAvaMediaModule::Get().GetOnAvaMediaPlaybackClientStarted().AddStatic(&FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackClient);
	}

	if (IStormSyncTransportServerModule::IsAvailable())
	{
		RegisterUserDataForPlaybackServer();
	}

	// Register the sync provider feature
	if (SyncProvider.IsValid())
	{
		IModularFeatures::Get().RegisterModularFeature(IAvaMediaSyncProvider::GetModularFeatureName(), SyncProvider.Get());
	}
}

void FStormSyncAvaBridgeModule::RegisterConsoleCommands()
{
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("StormSyncAvaBridge.Debug.GetUserData"),
		TEXT("Returns the associated value in Playback Server for ChannelName and Data Key"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FStormSyncAvaBridgeModule::ExecuteGetUserData),
		ECVF_Default
	));
}

void FStormSyncAvaBridgeModule::UnregisterConsoleCommands()
{
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}

	ConsoleCommands.Empty();
}

void FStormSyncAvaBridgeModule::ExecuteGetUserData(const TArray<FString>& Args)
{
	const FString Arguments = FString::Join(Args, TEXT(" "));
	STORM_SYNC_AVA_LOG(Display, TEXT("FStormSyncAvaBridgeModule::ExecuteGetUserData - Args: %s"), *Arguments)

	if (!Args.IsValidIndex(0) || Args[0].IsEmpty())
	{
		STORM_SYNC_AVA_LOG(Error, TEXT("Missing first parameter \"ChannelName\""))
		return;
	}

	if (!Args.IsValidIndex(1) || Args[1].IsEmpty())
	{
		STORM_SYNC_AVA_LOG(Error, TEXT("Missing second parameter \"Key\""))
		return;
	}

	if (!IAvaMediaModule::IsModuleLoaded())
	{
		STORM_SYNC_AVA_LOG(Error, TEXT("Ava Media Module is not available"))
		return;
	}

	if (!IAvaMediaModule::Get().IsMediaPlaybackClientStarted())
	{
		STORM_SYNC_AVA_LOG(Error, TEXT("Ava Playback Client stopped"))
		return;
	}

	const FString ChannelName = Args[0];
	const FString Key = Args[1];

	TArray<FString> ServerNames = FStormSyncAvaBridgeUtils::GetServerNamesForChannel(ChannelName);
	if (ServerNames.IsEmpty())
	{
		STORM_SYNC_AVA_LOG(Display, TEXT("Ava Broadcast profile for channel %s has no remotes"), *ChannelName)
		return;
	}

	const IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
	for (const FString& ServerName : ServerNames)
	{
		FString ServerValue = PlaybackClient.GetServerUserData(ServerName, Key);
		STORM_SYNC_AVA_LOG(Display, TEXT("\t Media Playback Client User Data for server %s - %s:%s"), *ServerName, *Key, *ServerValue);
	}
}

void FStormSyncAvaBridgeModule::OnStormSyncServerStarted()
{
	STORM_SYNC_AVA_LOG(Verbose, TEXT("FStormSyncAvaBridgeModule::OnStormSyncServerStarted"))
	RegisterUserDataForPlaybackServer();
}

void FStormSyncAvaBridgeModule::OnStormSyncServerStopped()
{
	STORM_SYNC_AVA_LOG(Verbose, TEXT("FStormSyncAvaBridgeModule::OnStormSyncServerStopped"))
	RegisterUserDataForPlaybackServer();
}

void FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackServer()
{
	// Fail safe checks for both Avalanche and Storm Sync modules, since this method can be executed from delegates in either of these modules
	if (!ValidateModulesAreAvailable())
	{
		return;
	}

	if (const TSharedPtr<FAvaMediaPlaybackServer> PlaybackServer = IAvaMediaModule::Get().GetMediaPlaybackServer())
	{
		const FString StormSyncServerAddress = IStormSyncTransportServerModule::Get().GetServerEndpointMessageAddressId();
		const FString DiscoveryManagerAddress = IStormSyncTransportServerModule::Get().GetDiscoveryManagerMessageAddressId();
		const FString StormSyncClientAddress = IStormSyncTransportClientModule::Get().GetClientEndpointMessageAddressId();

		PlaybackServer->SetUserData(UE::StormSync::AvaBridgeCommon::StormSyncServerAddressKey, StormSyncServerAddress);
		PlaybackServer->SetUserData(UE::StormSync::AvaBridgeCommon::StormSyncClientAddressKey, StormSyncClientAddress);
		PlaybackServer->SetUserData(UE::StormSync::AvaBridgeCommon::StormSyncDiscoveryAddressKey, DiscoveryManagerAddress);

		STORM_SYNC_AVA_LOG(Display, TEXT("FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackServer StormSyncServerAddress: %s"), *StormSyncServerAddress)
		STORM_SYNC_AVA_LOG(Display, TEXT("FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackServer StormSyncClientAddress: %s"), *StormSyncClientAddress)
		STORM_SYNC_AVA_LOG(Display, TEXT("FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackServer DiscoveryManagerAddress: %s"), *DiscoveryManagerAddress)
	}
}

void FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackClient()
{
	// Fail safe checks for both Avalanche and Storm Sync modules, since this method can be executed from delegates in either of these modules
	if (!ValidateModulesAreAvailable())
	{
		return;
	}

	if (IAvaMediaModule::Get().IsMediaPlaybackClientStarted())
	{
		IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();

		const FString StormSyncServerAddress = IStormSyncTransportServerModule::Get().GetServerEndpointMessageAddressId();
		const FString DiscoveryManagerAddress = IStormSyncTransportServerModule::Get().GetDiscoveryManagerMessageAddressId();
		const FString StormSyncClientAddress = IStormSyncTransportClientModule::Get().GetClientEndpointMessageAddressId();

		PlaybackClient.SetUserData(UE::StormSync::AvaBridgeCommon::StormSyncServerAddressKey, StormSyncServerAddress);
		PlaybackClient.SetUserData(UE::StormSync::AvaBridgeCommon::StormSyncClientAddressKey, StormSyncClientAddress);
		PlaybackClient.SetUserData(UE::StormSync::AvaBridgeCommon::StormSyncDiscoveryAddressKey, DiscoveryManagerAddress);

		STORM_SYNC_AVA_LOG(Display, TEXT("FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackClient StormSyncServerAddress: %s"), *StormSyncServerAddress)
		STORM_SYNC_AVA_LOG(Display, TEXT("FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackClient StormSyncClientAddress: %s"), *StormSyncClientAddress)
		STORM_SYNC_AVA_LOG(Display, TEXT("FStormSyncAvaBridgeModule::RegisterUserDataForPlaybackClient DiscoveryManagerAddress: %s"), *DiscoveryManagerAddress)
	}
}

bool FStormSyncAvaBridgeModule::ValidateModulesAreAvailable()
{
	if (!IAvaMediaModule::IsModuleLoaded())
	{
		STORM_SYNC_AVA_LOG(Warning, TEXT("FStormSyncAvaBridgeModule::ValidateModulesAreAvailable - Failed to set user data cause Ava Media Module is not loaded"))
		return false;
	}

	if (!IStormSyncTransportServerModule::IsAvailable())
	{
		STORM_SYNC_AVA_LOG(Warning, TEXT("FStormSyncAvaBridgeModule::ValidateModulesAreAvailable - Failed to set user data cause Storm Sync Server Module is not loaded"))
		return false;
	}

	if (!IStormSyncTransportClientModule::IsAvailable())
	{
		STORM_SYNC_AVA_LOG(Warning, TEXT("FStormSyncAvaBridgeModule::ValidateModulesAreAvailable - Failed to set user data cause Storm Sync Server Client is not loaded"))
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStormSyncAvaBridgeModule, StormSyncAvaBridge)
