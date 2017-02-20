// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveCreateMatchSession.h"
#include "OnlineAsyncTaskLiveSubmitMatchTicket.h"
#include "OnlineAsyncTaskLiveRegisterLocalUser.h"
#include "OnlineAsyncTaskLiveDestroySession.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

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

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveCreateMatchSession::FOnlineAsyncTaskLiveCreateMatchSession(class FOnlineSubsystemLive* InLiveSubsystem,
																		   const TArray< TSharedRef<const FUniqueNetId> >& InSearchingUserIds,
																		   FName InSessionName,
																		   const FOnlineSessionSettings& InSessionSettings,
																		   TSharedPtr<FOnlineSessionSearch>& InSearchSettings)
	: FOnlineAsyncTaskLiveSessionBase(InLiveSubsystem, INDEX_NONE, InSessionName, InSessionSettings)
	, SearchingUserIds(InSearchingUserIds)
	, SearchSettings(InSearchSettings)
	, CurrentMatchSessionRef(nullptr)
	, Association(nullptr)
	, bSessionCreated(false)
	, NumOtherLocalPlayersToAdd(0)
{	
	check(SearchingUserIds.Num() > 0);

	FUniqueNetIdLive FirstUserNetId(SearchingUserIds[0].Get());
	UserIndex = LiveSubsystem->GetIdentityLive()->GetControllerIndexForId(FirstUserNetId);
	SearchingUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(FirstUserNetId);
	
	ParseSearchSettings(InSearchSettings.Get());
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveCreateMatchSession::~FOnlineAsyncTaskLiveCreateMatchSession()
{
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void FOnlineAsyncTaskLiveCreateMatchSession::Start()
{
	// This task will make other local users join the match session after the first has created it.
	IAsyncOperation<MultiplayerSession^>^ WriteSessionOp =
		LiveSubsystem->GetSessionInterfaceLive()->CreateSessionOperation(SearchingUserIds[0].Get(), SessionSettings, FString(), SessionTemplateName);

	create_task( WriteSessionOp )
		.then([this](task<MultiplayerSession^> t)
	{    
		try
		{
			LiveSession = t.get(); // if t.get() didn't throw, it succeeded
			bSessionCreated = true;

			if (SearchingUserIds.Num() > 1)
			{
				// There are other local users that need to be added to the match session before submitting it.
				auto MatchmakingInterface = LiveSubsystem->GetMatchmakingInterfaceLive();
				check(MatchmakingInterface.IsValid());

				FOnlineMatchTicketInfoPtr MatchmakingTicket;
				MatchmakingInterface->GetMatchmakingTicket(SessionName, MatchmakingTicket);

				check(MatchmakingTicket.IsValid());

				MatchmakingTicket->RefreshLiveInfo(LiveSession);	// need to do this before running tasks on the session

				NumOtherLocalPlayersToAdd = SearchingUserIds.Num() - 1;
				bWasSuccessful = true;	// assume success unless a RegisterLocalPlayer delegate changes this

				// Start from index 1, since user 0 was already added when the session was created.
				for (int32 i = 1; i < SearchingUserIds.Num(); ++i)
				{
					FUniqueNetIdLive UserNetId(SearchingUserIds[i].Get());
					auto LiveContext = LiveSubsystem->GetLiveContext(UserNetId);

					auto RegisterTask = new FOnlineAsyncTaskLiveRegisterLocalUser(
						SessionName,
						LiveContext,
						LiveSubsystem,
						UserNetId,
						FOnRegisterLocalPlayerCompleteDelegate::CreateRaw(this, &FOnlineAsyncTaskLiveCreateMatchSession::OnAddLocalPlayerComplete));
					LiveSubsystem->GetAsyncTaskManager()->AddToParallelTasks(RegisterTask);
				}
			}
			else
			{
				// Complete, Finalize() will kick off the process of registering the match session and submitting
				// the match ticket.
				bWasSuccessful = true;
				bIsComplete = true;
			}
		}
		catch (Platform::Exception^ ex)
		{
			UE_LOG(LogOnline, Error, L"FOnlineAsyncTaskLiveStartMatchmaking::Start: matching session creation failed with 0x%0.8X", ex->HResult);
			bSessionCreated = false;
			bWasSuccessful = false;
			bIsComplete = true;
		}
	});
}

void FOnlineAsyncTaskLiveCreateMatchSession::OnAddLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result)
{
	FPlatformAtomics::InterlockedDecrement(&NumOtherLocalPlayersToAdd);

	if(Result != EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(LogOnline, Error, TEXT("FOnlineAsyncTaskLiveStartMatchmaking::OnAddLocalPlayerComplete: failed to add local player to match session with result %u"), static_cast<uint32>(Result));
		bWasSuccessful = false;
	}

	if(NumOtherLocalPlayersToAdd <= 0)
	{
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskLiveCreateMatchSession::Finalize()
{
	auto SessionInterface = LiveSubsystem->GetSessionInterfaceLive();
	check(SessionInterface.IsValid());

	if (!bWasSuccessful)
	{
		// Accounts for failure to add a local user to the session
		if (bSessionCreated)
		{
			// The session was created, but another member failed to join. The session needs to be destroyed
			// to clean up.
			CreateDestroyMatchmakingCompleteTask(SessionName, LiveSession, LiveSubsystem, false);
		}
		else
		{
			// this indicates a failure to create the Live match session. Remove the OSS named session
			// and trigger the delegate.
			auto MatchmakingInterface = LiveSubsystem->GetMatchmakingInterfaceLive();
			check(MatchmakingInterface.IsValid());
			MatchmakingInterface->RemoveMatchmakingTicket(SessionName);
		}

		return;
	}

	// Live match session creation succeeded. Update the SessionInfo and register & submit the matching ticket.
	check(LiveSession != nullptr);

	auto MatchmakingInterface = LiveSubsystem->GetMatchmakingInterfaceLive();
	check(MatchmakingInterface.IsValid());

	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	MatchmakingInterface->GetMatchmakingTicket(SessionName, MatchmakingTicket);
	// Abort if matchmaking was cancelled
	if (MatchmakingTicket.IsValid() == false || MatchmakingTicket->MatchmakingState == EOnlineLiveMatchmakingState::UserCancelled)
	{
		bWasSuccessful = false;
		return;
	}

	MatchmakingTicket->SessionSettings = SessionSettings;
	// Store actual match session
	MatchmakingTicket->RefreshLiveInfo(LiveSession);	// need to do this before running tasks on the session
	MatchmakingTicket->MatchmakingState = EOnlineLiveMatchmakingState::SubmittingInitialTicket;
	MatchmakingTicket->SetHostUser(SearchingUser);

	LiveSubsystem->GetMatchmakingInterfaceLive()->SubmitMatchingTicket(LiveSession->SessionReference, SessionName, false);

	// Initialize session state after create/join
	LiveSubsystem->GetSessionMessageRouter()->SyncInitialSessionState(SessionName, LiveSession);
}

void FOnlineAsyncTaskLiveCreateMatchSession::TriggerDelegates()
{
	// Account for failure to add a user to a created session
	if (!bWasSuccessful && !bSessionCreated)
	{
		// Only fire delegates on failure to create a session. It's unfortunate to break up
		// the logic like this, but on success the delegates will be fired in the GameSessionReady
		// task. If the session was created but a member join failed, the delegates will be fired
		// by the DestroySession task.
		auto MatchmakingInterface = LiveSubsystem->GetMatchmakingInterfaceLive();
		check(MatchmakingInterface.IsValid());

		MatchmakingInterface->TriggerOnMatchmakingCompleteDelegates(SessionName, bWasSuccessful);
	}
}
