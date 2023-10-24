// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "LiveLinkHubConnectionManager.h"
#include "Modules/ModuleManager.h"

#ifndef WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
#define WITH_LIVELINK_DISCOVERY_MANAGER_THREAD 1
#endif

#ifndef WITH_LIVELINK_HUB
#define WITH_LIVELINK_HUB 0
#endif

class FLiveLinkHubMessagingModule : public IModuleInterface
{
	// The connection manager is used to communicate with the hub, so we don't need it when we're running the hub itself.
#if !WITH_LIVELINK_HUB && WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override
	{
		ConnectionManager = MakeUnique<FLiveLinkHubConnectionManager>();
	}

	virtual void ShutdownModule() override
	{
		ConnectionManager.Reset();
	}
	//~ End IModuleInterface

private:
	/** Manages the connection to the live link hub. */
	TUniquePtr<FLiveLinkHubConnectionManager> ConnectionManager;
#endif
};

IMPLEMENT_MODULE(FLiveLinkHubMessagingModule, LiveLinkHubMessaging);
