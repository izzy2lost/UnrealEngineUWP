// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"

class FOnlineSubsystemLive;

/**
 * Async task that creates a matchmaking ticket and submits it.
 */
class FOnlineAsyncTaskLiveSubmitMatchTicket : public FOnlineAsyncTaskLive
{
public:
	FOnlineAsyncTaskLiveSubmitMatchTicket(
		FOnlineSubsystemLive* InSubsystem,
		Windows::Xbox::System::User^ InSearchingUser,
		FName InSessionName,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InMatchSessionRef,
		Platform::String^ InHopperName,
		Platform::String^ InTicketAttributes,
		Windows::Foundation::TimeSpan InTicketTimeout,
		Microsoft::Xbox::Services::Matchmaking::PreserveSessionMode InTicketPreservation,
		bool InCancelExistingTicket);

	virtual void Initialize() override;

	virtual FString ToString() const override { return TEXT("SubmitMatchTicket"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	void CreateMatchmakingTicket();

	Windows::Xbox::System::User^ SearchingUser;
	FName SessionName;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ MatchSessionRef;
	Platform::String^ HopperName;
	Platform::String^ TicketAttributes;
	Windows::Foundation::TimeSpan TicketTimeout;
	Microsoft::Xbox::Services::Matchmaking::PreserveSessionMode TicketPreservation;
	bool CancelExistingTicket;

	FString TicketIdToCancel;
	Microsoft::Xbox::Services::Matchmaking::CreateMatchTicketResponse^ Response;
};

//------------------------------- End of file ---------------------------------
