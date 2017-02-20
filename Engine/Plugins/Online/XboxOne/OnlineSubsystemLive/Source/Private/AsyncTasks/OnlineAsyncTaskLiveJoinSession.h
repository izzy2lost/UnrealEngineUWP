// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineSessionInterface.h"

class FOnlineSubsystemLive;

/** 
 * Async item used to marshal a join request from the system callback thread to the game thread.
 */
class FOnlineAsyncTaskLiveJoinSession : public FOnlineAsyncTaskBasic<FOnlineSubsystemLive>
{
public:
	FOnlineAsyncTaskLiveJoinSession(
			class FOnlineSessionLive* InLiveInterface,
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InReference,
			Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ InTemplate,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			class FNamedOnlineSession* InNamedSession,
			class FOnlineSubsystemLive* Subsystem,
			int RetryCount);

	// Handle joining a matchmaking target session prior to QoS
	FOnlineAsyncTaskLiveJoinSession(
			class FOnlineSessionLive* InLiveInterface,
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InReference,
			Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ InTemplate,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			class FNamedOnlineSession* InNamedSession,
			class FOnlineSubsystemLive* Subsystem,
			int RetryCount,
			
			bool bSessionIsMatchmakingResult);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("JoinSessionAsync");}
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	void OnFailed(EOnJoinSessionCompleteResult::Type Result);
	void Retry(bool bGetSession);
	void TryJoinSession();
	
	// Callback when other local players are added to session in matchmaking case
	void OnAddLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result);
	
	class FOnlineSessionLive* SessionInterface;
	class FNamedOnlineSession* NamedSession;
	class FOnlineSubsystemLive* LiveSubsystem;

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference;
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ PeerTemplate;
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;

	// Saved values used in Finalize()
	Windows::Xbox::Networking::SecureDeviceAssociation^ Association;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession;

	EOnJoinSessionCompleteResult::Type JoinResult;

	int RetryCount;
	bool bIsMatchmakingResult;
	volatile int32 OtherLocalPlayersToAdd;
};
