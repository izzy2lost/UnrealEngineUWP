// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveSubmitMatchTicket.h"
#include "OnlineAsyncTaskLiveCancelMatchmaking.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "OnlineAsyncTaskLiveSetSessionActivity.h"

#include <collection.h>

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace concurrency;

using namespace Platform;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::System;

FOnlineAsyncTaskLiveSubmitMatchTicket::FOnlineAsyncTaskLiveSubmitMatchTicket(
	FOnlineSubsystemLive* InSubsystem,
	Windows::Xbox::System::User^ InSearchingUser,
	FName InSessionName,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InMatchSessionRef,
	Platform::String^ InHopperName,
	Platform::String^ InTicketAttributes,
	TimeSpan InTicketTimeout,
	Microsoft::Xbox::Services::Matchmaking::PreserveSessionMode InTicketPreservation,
	bool InCancelExistingTicket
)
	: FOnlineAsyncTaskLive(InSubsystem, 0)
	, SearchingUser(InSearchingUser)
	, SessionName(InSessionName)
	, MatchSessionRef(InMatchSessionRef)
	, HopperName(InHopperName)
	, TicketAttributes(InTicketAttributes)
	, TicketTimeout(InTicketTimeout)
	, TicketPreservation(InTicketPreservation)
	, CancelExistingTicket(InCancelExistingTicket)
{
	// Find the existing ticket id on the game thread if we need to cancel it
	if (CancelExistingTicket)
	{
		FOnlineMatchTicketInfoPtr CurrentTicket;
		Subsystem->GetMatchmakingInterfaceLive()->GetMatchmakingTicket(SessionName, CurrentTicket);

		if (CurrentTicket.IsValid())
		{
			TicketIdToCancel = CurrentTicket->TicketId;
		}
	}
}

void FOnlineAsyncTaskLiveSubmitMatchTicket::Initialize()
{
	if (CancelExistingTicket)
	{
		XboxLiveContext^ Context = Subsystem->GetLiveContext(SearchingUser);

		check(Context != nullptr);

		Platform::String^ TicketIdHat = ref new Platform::String(*TicketIdToCancel);
		// @ATG_CHANGE : BEGIN - UWP LIVE support
		auto DeleteTicketOp = Context->MatchmakingService->DeleteMatchTicketAsync(
			Context->AppConfig->ServiceConfigurationId,
			HopperName,
			TicketIdHat);
		// @ATG_CHANGE : END
		
		create_task(DeleteTicketOp).then([this, TicketIdHat](task<void> Task)
		{
			try
			{
				Task.get();

				UE_LOG_ONLINE(Log, TEXT("FOnlineAsyncTaskLiveSubmitMatchTicket: successfully deleted ticket %s."), TicketIdHat->Data());
			}
			catch(Platform::COMException^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskLiveSubmitMatchTicket: failed to cancel existing ticket."));
			}

			CreateMatchmakingTicket();
		});
	}
	else
	{
		CreateMatchmakingTicket();
	}
}

void FOnlineAsyncTaskLiveSubmitMatchTicket::CreateMatchmakingTicket()
{
	check(SearchingUser != nullptr);

	XboxLiveContext^ Context = Subsystem->GetLiveContext(SearchingUser);
	Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveSetSessionActivity>(Subsystem, Context, MatchSessionRef);
	
	// @ATG_CHANGE : BEGIN - UWP LIVE support
	auto createMatchTicketOperation = Context->MatchmakingService->CreateMatchTicketAsync(
		MatchSessionRef,
		Context->AppConfig->ServiceConfigurationId,
		HopperName,
		TicketTimeout,
		TicketPreservation,
		TicketAttributes);
	// @ATG_CHANGE : END
	create_task( createMatchTicketOperation )
		.then( [this] (task<CreateMatchTicketResponse^> t)
	{
		try
		{
			Response = t.get(); // if t.get() didn't throw, it succeeded

			bWasSuccessful = true;

			// Matchmaking is now running. When a match is found, the title will be notified via the session change
			// subscription, which will trigger OnSessionChanged.
			UE_LOG(LogOnline, Log, TEXT("Matchmaking ticket created... (%s)"), Response->MatchTicketId->Data() );
		}
		catch (Platform::Exception^ ex)
		{
			UE_LOG(LogOnline, Error, TEXT("CreateMatchTicketAsync failed with 0x%0.8X"), ex->HResult);

			bWasSuccessful = false;
		}

		bIsComplete = true;
	});
}

void FOnlineAsyncTaskLiveSubmitMatchTicket::Finalize()
{
	// Get the Live session info
	// [12/15/2014]: Split Matchmaking and Session Management
	FOnlineMatchTicketInfoPtr Ticket;
	// If the named session is null, it may have already been destroyed

	Subsystem->GetMatchmakingInterfaceLive()->GetMatchmakingTicket(SessionName, Ticket);


	//. Store the TicketID

	// Abort if matchmaking was canceled
	if (Ticket.IsValid() == false || Ticket->MatchmakingState == EOnlineLiveMatchmakingState::UserCancelled)
	{
		bWasSuccessful = false;

		// Queue the task here since queued tasks can start ticking before the previous task has run Finalize().
		// @todo: fix so that Finalize() is guaranteed to finish first?
		XboxLiveContext^ UserContext = Subsystem->GetLiveContext(SearchingUser);
		FOnlineAsyncTaskLiveCancelMatchmaking* CancelMatchTask = new FOnlineAsyncTaskLiveCancelMatchmaking(
				Subsystem,
				UserContext,
				SessionName,
				Ticket);
		Subsystem->QueueAsyncTask(CancelMatchTask);
		return;
	}

	if (!bWasSuccessful)
	{
		Subsystem->GetMatchmakingInterfaceLive()->RemoveMatchmakingTicket(SessionName);
		return;
	}
	Ticket->HopperName = HopperName->Data();
	Ticket->TicketId = Response->MatchTicketId->Data();
	Ticket->EstimatedWaitTimeInSeconds = ((float) Response->EstimatedWaitTime.Duration ) / 10000000;
	Ticket->MatchmakingState = EOnlineLiveMatchmakingState::WaitingForGameSession;
}

void FOnlineAsyncTaskLiveSubmitMatchTicket::TriggerDelegates()
{
	if (!bWasSuccessful)
	{
		// Only fire delegates on failure. It's unfortunate to break up the logic like this,
		// but on success the delegates will be fired when the session interface completes
		// the initialization flow in OnSessionChanged.
		auto MatchmakingInterface = Subsystem->GetMatchmakingInterfaceLive();
		check(MatchmakingInterface.IsValid());

		MatchmakingInterface->TriggerOnMatchmakingCompleteDelegates(SessionName, bWasSuccessful);
	}
}
