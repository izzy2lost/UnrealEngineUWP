// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../OnlineAsyncTaskManagerLive.h"
#include "IPAddress.h"


class FOnlineSubsystemLive;

/**
 * Async task to connect to users in a session after matchmaking and initialization/QoS is complete.
 */
class FOnlineAsyncTaskLiveGameSessionReady : public FOnlineAsyncTaskLive
{
public:
	FOnlineAsyncTaskLiveGameSessionReady(
		FOnlineSubsystemLive* InSubsystem,
		Microsoft::Xbox::Services::XboxLiveContext^ InContext,
		FName InSessionName,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InGameSessionRef);

	virtual void Initialize() override;

	virtual FString ToString() const override { return TEXT("GameSessionReady"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;
	FName SessionName;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ GameSessionRef;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession;
	Windows::Xbox::Networking::SecureDeviceAssociation^ Association;

	TSharedPtr<FInternetAddr> HostAddr;
};

//------------------------------- End of file ---------------------------------
