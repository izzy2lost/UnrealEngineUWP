// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "Interfaces/OnlineSessionInterface.h"

// @ATG_CHANGE : xbox headers moved to primary include so as to not repeat uwp\xbox branching
//#include "XboxOneAllowPlatformTypes.h"
//#include <collection.h>
//#include "XboxOneHidePlatformTypes.h"

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
			int RetryCount,
			bool bSessionIsMatchmakingResult,
			const bool bInSetActivity,
			const TOptional<FString>& InSessionInviteHandle);

	// FOnlineAsyncItem
	virtual FString ToString() const override {
		const bool WasSuccessful = bWasSuccessful;
		return FString::Printf(TEXT("FOnlineAsyncTaskLiveJoinSession SessionName: %s SessionId: %ls bWasSuccessful: %d"), *SessionName.ToString(), SessionReference->ToUriPath()->Data(), WasSuccessful); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	void OnSuccess();
	void OnFailed(EOnJoinSessionCompleteResult::Type Result);
	void Retry(bool bGetSession);
	void TryJoinSession();
	void TryJoinSessionFromMatchmaking(Platform::Collections::Vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^>^ LocalMembers);
	void TryJoinSessionFromDedicated();
	void TryJoinSessionFromPeer(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^ Host);
	
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
	bool bSetActivity;
	FName SessionName;
	TOptional<FString> SessionInviteHandle;
};
