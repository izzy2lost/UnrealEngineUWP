// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SessionMessageRouter.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineDelegateMacros.h"

#include "Interfaces/OnlineSessionInterface.h"

class FOnlineMatchmakingInterfaceLive
{
public:
	FOnlineMatchmakingInterfaceLive(class FOnlineSubsystemLive* InSubsystem);
	~FOnlineMatchmakingInterfaceLive();

PACKAGE_SCOPE:

	bool StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const class FOnlineSessionSettings& NewSessionSettings, TSharedRef<class FOnlineSessionSearch>& SearchSettings);

	bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName);
	bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName);
	
	/**
	 * Matchmaking related APIs
	 */
	void AddMatchmakingTicket( FName SessionName, FOnlineMatchTicketInfoPtr TicketInfo );
	void RemoveMatchmakingTicket( FName SessionName );
	bool GetMatchmakingTicket( FName SessionName, FOnlineMatchTicketInfoPtr& OutTicketInfo );
	void SetTicketState( FName SessionName, EOnlineLiveMatchmakingState::Type State);

	/** Resubmit a matchmaking ticket if necessary */
	void SubmitMatchingTicket(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionRef,
		FName SessionName,
		bool CancelExistingTicket);

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

	EOnlineLiveMatchmakingState::Type GetMatchmakingState(FName SessionName);
	void SetMatchmakingState(FName SessionName, EOnlineLiveMatchmakingState::Type State);

	void OnMultiplayerSubscriptionsLost();
	void OnSessionChanged(FName SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes Diff);
	
	/** Handle changes to matchmaking status */
	void OnMatchmakingStatusChanged(FName SessionName);
	void OnMemberListChanged(FName SessionName);

	FOnSessionChangedDelegate OnSessionChangedDelegate;

	FDelegateHandle OnSubscriptionLostDelegateHandle;
	FDelegateHandle OnSessionChangedDelegateHandle;

	typedef TMap<FName, FOnlineMatchTicketInfoPtr> TicketInfoMap;
	mutable FCriticalSection	TicketsLock;
	TicketInfoMap				MatchmakingTickets;

	class FOnlineSubsystemLive* LiveSubsystem;
};