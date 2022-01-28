// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SessionMessageRouter.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineDelegateMacros.h"

#include "Interfaces/OnlineSessionInterface.h"

class FOnlineSubsystemLive;
class FOnlineSessionSettings;
class FOnlineSessionSearch;

class FOnlineMatchmakingInterfaceLive
{
public:
	FOnlineMatchmakingInterfaceLive(FOnlineSubsystemLive* InSubsystem);
	~FOnlineMatchmakingInterfaceLive();

PACKAGE_SCOPE:

	bool StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, const FName SessionName, const FOnlineSessionSettings& NewSessionSettings, const TSharedRef<FOnlineSessionSearch>& SearchSettings);

	bool CancelMatchmaking(const int32 SearchingPlayerNum, const FName SessionName);
	bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, const FName SessionName);
	
	/**
	 * Matchmaking related APIs
	 */
	void AddMatchmakingTicket(const FName SessionName, const FOnlineMatchTicketInfoPtr TicketInfo);
	void RemoveMatchmakingTicket(const FName SessionName );
	bool GetMatchmakingTicket(const FName SessionName, FOnlineMatchTicketInfoPtr& OutTicketInfo) const;
	void SetTicketState(const FName SessionName, const EOnlineLiveMatchmakingState::Type State);

	/** Resubmit a matchmaking ticket if necessary */
	void SubmitMatchingTicket(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionRef,
		const FName SessionName,
		const bool bCancelExistingTicket);

	/** Look up the ticket corresponding to a Live session reference */
	FOnlineMatchTicketInfoPtr GetMatchTicketForLiveSessionRef(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LiveSessionRef);

	/**
	 * Delegate fired when the cloud matchmaking has completed
	 *
	 * @param SessionName The name of the session that was found via matchmaking
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnMatchmakingComplete, FName, bool);

		/**
	 * Delegate fired when the cloud matchmaking has been canceled
	 *
	 * @param SessionName the name of the session that was canceled
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnCancelMatchmakingComplete, FName, bool);

private:

	EOnlineLiveMatchmakingState::Type GetMatchmakingState(FName SessionName) const;
	void SetMatchmakingState(const FName SessionName, const EOnlineLiveMatchmakingState::Type State);

	void OnMultiplayerSubscriptionsLost();
	void OnSessionChanged(const FName SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes Diff);
	
	/** Handle changes to matchmaking status */
	void OnMatchmakingStatusChanged(const FName SessionName);
	void OnMemberListChanged(const FName SessionName);

	FOnSessionChangedDelegate OnSessionChangedDelegate;

	FDelegateHandle OnSubscriptionLostDelegateHandle;
	FDelegateHandle OnSessionChangedDelegateHandle;

	typedef TMap<FName, FOnlineMatchTicketInfoPtr> TicketInfoMap;
	mutable FCriticalSection	TicketsLock;
	TicketInfoMap				MatchmakingTickets;

	FOnlineSubsystemLive* LiveSubsystem;
};

typedef TSharedPtr<FOnlineMatchmakingInterfaceLive, ESPMode::ThreadSafe> FOnlineMatchmakingInterfaceLivePtr;
