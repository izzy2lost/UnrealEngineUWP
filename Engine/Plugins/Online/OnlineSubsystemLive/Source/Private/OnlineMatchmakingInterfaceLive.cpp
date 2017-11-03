// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineMatchmakingInterfaceLive.h"	

#include "OnlineSubsystemLive.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineIdentityInterfaceLive.h"
#include "SessionMessageRouter.h"
#include "OnlineSessionInterfaceLive.h"
#include "Misc/ScopeLock.h"

#include "AsyncTasks/OnlineAsyncTaskLiveCreateSession.h"
#include "AsyncTasks/OnlineAsyncTaskLiveFindSessions.h"
#include "AsyncTasks/OnlineAsyncTaskLiveJoinSession.h"
#include "AsyncTasks/OnlineAsyncTaskLiveCreateMatchSession.h"
#include "AsyncTasks/OnlineAsyncTaskLiveSubmitMatchTicket.h"
#include "AsyncTasks/OnlineAsyncTaskLiveGameSessionReady.h"
#include "AsyncTasks/OnlineAsyncTaskLiveCancelMatchmaking.h"
#include "AsyncTasks/OnlineAsyncTaskLiveDestroySession.h"

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::System;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Social;
using namespace concurrency;


//////////////////////////////////////////////////////////////////////////
FOnlineMatchmakingInterfaceLive::FOnlineMatchmakingInterfaceLive(class FOnlineSubsystemLive* InSubsystem)
	: LiveSubsystem(InSubsystem)
{
	OnSessionChangedDelegate = FOnSessionChangedDelegate::CreateRaw(this, &FOnlineMatchmakingInterfaceLive::OnSessionChanged);

	OnSubscriptionLostDelegateHandle = LiveSubsystem->GetSessionMessageRouter()->OnSubscriptionLostDelegates.AddRaw(this, &FOnlineMatchmakingInterfaceLive::OnMultiplayerSubscriptionsLost);
}

//////////////////////////////////////////////////////////////////////////
FOnlineMatchmakingInterfaceLive::~FOnlineMatchmakingInterfaceLive()
{
	LiveSubsystem->GetSessionMessageRouter()->ClearOnSubscriptionLostDelegate_Handle(OnSubscriptionLostDelegateHandle);
}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::OnMultiplayerSubscriptionsLost()
{

}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::OnSessionChanged(FName SessionName, MultiplayerSessionChangeTypes Diff)
{
	UE_LOG_ONLINE(Log, TEXT("FOnlineMatchmakingInterfaceLive::OnSessionChanged"));

	if ((Diff & MultiplayerSessionChangeTypes::MatchmakingStatusChange) == MultiplayerSessionChangeTypes::MatchmakingStatusChange)
	{
		OnMatchmakingStatusChanged(SessionName);
	}
	if ((Diff & MultiplayerSessionChangeTypes::MemberListChange) == MultiplayerSessionChangeTypes::MemberListChange)
	{
		OnMemberListChanged(SessionName);
	}
}

//////////////////////////////////////////////////////////////////////////
bool FOnlineMatchmakingInterfaceLive::StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	if (LocalPlayers.Num() == 0)
	{
		UE_LOG_ONLINE(Error, TEXT("LocalPlayers was empty. At least one player is required for matchmaking."));
		TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}


	auto SearchSettingsPtr = TSharedPtr<FOnlineSessionSearch>( SearchSettings );

	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	GetMatchmakingTicket(SessionName, MatchmakingTicket);

	if (MatchmakingTicket.IsValid() == false)
	{
		MatchmakingTicket = MakeShared<FOnlineMatchTicketInfo>();
		AddMatchmakingTicket(SessionName, MatchmakingTicket);
	}

	MatchmakingTicket->MatchmakingState = EOnlineLiveMatchmakingState::CreatingMatchSession;
	MatchmakingTicket->SessionName = SessionName;
	MatchmakingTicket->SessionSearch = SearchSettings;
	MatchmakingTicket->SessionSettings = NewSessionSettings;
	

	FOnlineAsyncTaskLiveCreateMatchSession* StartMatchTask = 
		new FOnlineAsyncTaskLiveCreateMatchSession(
		LiveSubsystem,
		LocalPlayers,
		SessionName,
		NewSessionSettings,
		SearchSettingsPtr
		);

	bool bSettingsValid = StartMatchTask->SettingsAreValid();

	if ( bSettingsValid )
	{
		LiveSubsystem->QueueAsyncTask(StartMatchTask);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineMatchmakingInterfaceLive::StartMatchmaking: session settings are invalid."));
		TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		delete StartMatchTask;
	}

	return bSettingsValid;
}

//////////////////////////////////////////////////////////////////////////
bool FOnlineMatchmakingInterfaceLive::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) 
{
	UE_LOG(LogOnline, Log, L"LIVE Cancel Matchmaking %s", *SessionName.ToString() );

	TSharedPtr<const FUniqueNetId> UserId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(SearchingPlayerNum);
	if(!UserId.IsValid())
	{
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return true;
	}

	return CancelMatchmaking(*UserId, SessionName);
}

//////////////////////////////////////////////////////////////////////////
EOnlineLiveMatchmakingState::Type FOnlineMatchmakingInterfaceLive::GetMatchmakingState(FName SessionName)
{
	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	GetMatchmakingTicket(SessionName, MatchmakingTicket);

	if (MatchmakingTicket.IsValid())
	{
		return MatchmakingTicket->MatchmakingState;
	}

	//Fall back to session?

	return EOnlineLiveMatchmakingState::None;
}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::SetMatchmakingState(FName SessionName, EOnlineLiveMatchmakingState::Type State)
{
	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	GetMatchmakingTicket(SessionName, MatchmakingTicket);

	if (MatchmakingTicket.IsValid())
	{
		MatchmakingTicket->MatchmakingState = State;
	}
}

//////////////////////////////////////////////////////////////////////////
bool FOnlineMatchmakingInterfaceLive::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
	UE_LOG(LogOnline, Log, L"LIVE Cancel Matchmaking %s", *SessionName.ToString() );

	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	GetMatchmakingTicket(SessionName, MatchmakingTicket);

	if (MatchmakingTicket.IsValid() == false)
	{
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return true;
	}

	switch (MatchmakingTicket->MatchmakingState)
	{
		case EOnlineLiveMatchmakingState::None:
			{
				// Session is not in a valid state to be canceled
				RemoveMatchmakingTicket(SessionName);
				TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
				return true;
			}
		
		case EOnlineLiveMatchmakingState::SubmittingInitialTicket:
		case EOnlineLiveMatchmakingState::CreatingMatchSession:
			{
				RemoveMatchmakingTicket(SessionName);
				TriggerOnCancelMatchmakingCompleteDelegates(SessionName, true);
				return true;
			}

		default:
			{
				MatchmakingTicket->MatchmakingState = EOnlineLiveMatchmakingState::UserCancelled;
			}
	}

	XboxLiveContext^ UserContext = LiveSubsystem->GetLiveContext(static_cast<const FUniqueNetIdLive&>(SearchingPlayerId));

	FOnlineAsyncTaskLiveCancelMatchmaking* CancelMatchTask = 
		new FOnlineAsyncTaskLiveCancelMatchmaking(
		LiveSubsystem,
		UserContext,
		SessionName,
		MatchmakingTicket);

	LiveSubsystem->QueueAsyncTask(CancelMatchTask);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::AddMatchmakingTicket( FName SessionName, FOnlineMatchTicketInfoPtr InTicketInfo  )
{
	FScopeLock ScopeLock( &TicketsLock );
	MatchmakingTickets.Add( SessionName, InTicketInfo );
}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::RemoveMatchmakingTicket( FName SessionName )
{
	FScopeLock ScopeLock( &TicketsLock );

	if ( FOnlineMatchTicketInfoPtr* FoundTicket = MatchmakingTickets.Find( SessionName ) )
	{
		LiveSubsystem->GetSessionMessageRouter()->ClearOnSessionChangedDelegate(OnSessionChangedDelegate, (*FoundTicket)->GetLiveSessionRef());
	}


	MatchmakingTickets.Remove( SessionName );
}

//////////////////////////////////////////////////////////////////////////
bool FOnlineMatchmakingInterfaceLive::GetMatchmakingTicket( FName SessionName, FOnlineMatchTicketInfoPtr& OutTicketInfo  )
{
	FScopeLock ScopeLock( &TicketsLock );

	const FOnlineMatchTicketInfoPtr* FoundTicket = MatchmakingTickets.Find( SessionName );

	if ( FoundTicket )
	{
		OutTicketInfo = *FoundTicket;
	}

	return ( FoundTicket != nullptr );
}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::SetTicketState( FName SessionName, EOnlineLiveMatchmakingState::Type State)
{
	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	GetMatchmakingTicket(SessionName, MatchmakingTicket);

	if (MatchmakingTicket.IsValid())
	{
		MatchmakingTicket->MatchmakingState = State;
	}
}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::OnMatchmakingStatusChanged(FName SessionName)
{
	check(IsInGameThread());

	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	GetMatchmakingTicket(SessionName, MatchmakingTicket);

	if (MatchmakingTicket.IsValid() == false)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineMatchmakingInterfaceLive::OnMatchmakingStatusChanged - ticket doesn't exist or was destroyed before task ran"));
		return;
	}

	MultiplayerSession^ LiveSession = MatchmakingTicket->GetLiveSession();

	MatchmakingStatus MatchStatus = LiveSession->MatchmakingServer->Status;

	switch (MatchStatus)
	{
	case MatchmakingStatus::Unknown:
		UE_LOG_ONLINE(Log, TEXT("  MatchStatus = Unknown"));
		break;

	case MatchmakingStatus::None:
		UE_LOG_ONLINE(Log, TEXT("  MatchStatus = None"));
		break;

	case MatchmakingStatus::Searching:
		UE_LOG_ONLINE(Log, TEXT("  MatchStatus = Searching"));
		break;

	case MatchmakingStatus::Expired:
		UE_LOG_ONLINE(Log, TEXT("  MatchStatus = Expired"));

		SubmitMatchingTicket(
			LiveSession->SessionReference,
			SessionName,
			false);
		break;

	case MatchmakingStatus::Found:
		{
			UE_LOG_ONLINE(Log, TEXT("  MatchStatus = Found"));

			// Join the target session so we can do QoS. If this isn't a match session (we're
			// advertising an existing game session), we've already joined.
			FOnlineSessionLivePtr SessionInterface = LiveSubsystem->GetSessionInterfaceLive();
			if (SessionInterface->GetNamedSession(SessionName) == nullptr || SessionName == PartySessionName)
			{
				Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ TargetSessionReference =
					LiveSession->MatchmakingServer->TargetSessionRef;
				check(TargetSessionReference);

				LiveSubsystem->GetSessionMessageRouter()->ClearOnSessionChangedDelegate(OnSessionChangedDelegate, LiveSession->SessionReference);
				MatchmakingTicket->RefreshLiveInfo(TargetSessionReference);
				MatchmakingTicket->SetLastDiffedSession(nullptr);
				
				auto LiveContext = LiveSubsystem->GetLiveContext(LiveSession);

				// Create a named session from the search result data
				FNamedOnlineSession* NamedSession = SessionInterface->AddNamedSession(GameSessionName, MatchmakingTicket->SessionSettings);
				NamedSession->HostingPlayerNum = INDEX_NONE;
				{
					//Party tickets should now follow the Matchmade session for backfilling
					MatchmakingTicket->SessionName = GameSessionName;
					
					FScopeLock ScopeLock( &TicketsLock );
					MatchmakingTickets.Remove( SessionName );
					MatchmakingTickets.Add(GameSessionName, MatchmakingTicket);					
				}

				//Record the game session URI so invites can pass along the target session.
				if (SessionName == PartySessionName) 
				{
					if (auto NamedSession = SessionInterface->GetNamedSession(SessionName))
					{
						NamedSession->SessionSettings.Set(SETTING_GAME_SESSION_URI, FString(TargetSessionReference->ToUriPath()->Data()), EOnlineDataAdvertisementType::ViaOnlineService);
						SessionInterface->UpdateSession(SessionName, NamedSession->SessionSettings, true);
					}
				}

				NamedSession->SessionInfo = MakeShared<FOnlineSessionInfoLive>(TargetSessionReference);
				
				LiveSubsystem->GetSessionMessageRouter()->AddOnSessionChangedDelegate(SessionInterface->OnSessionChangedDelegate, TargetSessionReference);

				UE_LOG_ONLINE(Log, TEXT("Session Found: %s %s"), TargetSessionReference->SessionTemplateName->Data(), TargetSessionReference->SessionName->Data());

				const bool bSessionIsMatchmakingResult = true;
				const bool bInSetActivity = true;

				LiveSubsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveJoinSession>(SessionInterface.Get(),
					TargetSessionReference,
					nullptr,
					LiveContext,
					NamedSession,
					LiveSubsystem,
					SessionInterface->MAX_RETRIES,
					bSessionIsMatchmakingResult,
					bInSetActivity);
			}
			break;
		}

	case MatchmakingStatus::Canceled:
		
		if (MatchmakingTicket.IsValid() && MatchmakingTicket->MatchmakingState != EOnlineLiveMatchmakingState::UserCancelled)
		{
			SubmitMatchingTicket(
				LiveSession->SessionReference,
				SessionName,
				false);
		}

		UE_LOG_ONLINE(Log, TEXT("  MatchStatus = Canceled"));
		break;

	default:
		UE_LOG_ONLINE(Warning, TEXT("FOnlineMatchmakingInterfaceLive::OnMatchmakingStatusChanged - Got unexpected MatchmakingStatus: %u"), MatchStatus);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::SubmitMatchingTicket(
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionRef,
	FName SessionName,
	bool CancelExistingTicket)
{

	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	GetMatchmakingTicket(SessionName, MatchmakingTicket);

	FNamedOnlineSession* NamedSession = LiveSubsystem->GetSessionInterfaceLive()->GetNamedSession(SessionName);

	//Start matchmaking should have been called first...
	if (MatchmakingTicket.IsValid() == false)
	{
		return;
	}

	LiveSubsystem->GetSessionMessageRouter()->AddOnSessionChangedDelegate(OnSessionChangedDelegate, SessionRef);

	if (NamedSession && !NamedSession->SessionSettings.bShouldAdvertise)
	{
		return;
	}

	// Don't submit if not matchmaking
	if (MatchmakingTicket->MatchmakingState == EOnlineLiveMatchmakingState::None)
	{
		return;
	}

	// When the game is active, only the host should be submitting tickets
	if ((MatchmakingTicket->MatchmakingState == EOnlineLiveMatchmakingState::Active) && NamedSession && NamedSession->OwningUserId.IsValid())
	{
		if (!LiveSubsystem->IsLocalPlayer(*NamedSession->OwningUserId))
		{
			return;
		}
	}

	FOnlineSessionInfoLive* LiveInfo =  NamedSession && NamedSession->SessionInfo.IsValid() ? static_cast<FOnlineSessionInfoLive*>(NamedSession->SessionInfo.Get()) : nullptr;

	if (LiveInfo && LiveInfo->GetLiveMultiplayerSession() != nullptr)
	{
		uint32 CurrentSessionSize = LiveInfo->GetLiveMultiplayerSession()->Members->Size;
		if( CurrentSessionSize >= (uint32) NamedSession->SessionSettings.NumPublicConnections )
		{
			UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::SubmitMatchingTicket: Maximum players reached for this session. No longer matchmaking. Size: %d"), CurrentSessionSize);
			return;
		}
	}

	auto SearchSettings = MatchmakingTicket->SessionSearch;
	check(SearchSettings.IsValid());

	FString MatchHopperName;
	SearchSettings->QuerySettings.Get(SEARCH_XBOX_LIVE_HOPPER_NAME, MatchHopperName);
	MatchmakingTicket->HopperName = MatchHopperName;

	FString MatchTicketAttributes;
	const FOnlineSessionSetting* AttributesSetting =  MatchmakingTicket->SessionSettings.Settings.Find(SETTING_MATCHING_ATTRIBUTES);
	if ( AttributesSetting )
	{
		AttributesSetting->Data.GetValue( MatchTicketAttributes );
	}

	TimeSpan PlatformTimeout;
	PlatformTimeout.Duration	= (uint64) (10000000 * SearchSettings->TimeoutInSeconds); //. Convert timeout to duration in ticks (10 million per second)

	PreserveSessionMode Mode = PreserveSessionMode::Always;

	bool CreateNewSession = false;
	if (NamedSession && NamedSession->SessionSettings.Get(SETTING_MATCHING_PRESERVE_SESSION, CreateNewSession))
	{
		CreateNewSession = !CreateNewSession;
	}

	// If we don't have a game session yet, set the ticket to create one
	if (LiveInfo == nullptr || LiveInfo->GetLiveMultiplayerSession() == nullptr || CreateNewSession)
	{
		Mode = PreserveSessionMode::Never;
	}

	FOnlineAsyncTaskLiveSubmitMatchTicket* SubmitTask = new FOnlineAsyncTaskLiveSubmitMatchTicket(
		LiveSubsystem,
		MatchmakingTicket->GetHostUser(),
		SessionName,
		SessionRef,
		ref new Platform::String(*MatchHopperName),
		ref new Platform::String(*MatchTicketAttributes),
		PlatformTimeout,
		Mode,
		CancelExistingTicket);
	LiveSubsystem->QueueAsyncTask(SubmitTask);
}

//////////////////////////////////////////////////////////////////////////
void FOnlineMatchmakingInterfaceLive::OnMemberListChanged(FName SessionName)
{
	check(IsInGameThread());

	FOnlineMatchTicketInfoPtr MatchmakingTicket;
	GetMatchmakingTicket(SessionName, MatchmakingTicket);

	if (MatchmakingTicket.IsValid() == false)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineMatchmakingInterfaceLive::OnMatchmakingStatusChanged - session doesn't exist or was destroyed before task ran"));
		return;
	}

	if (FNamedOnlineSession* NamedSession = LiveSubsystem->GetSessionInterfaceLive()->GetNamedSession(SessionName))
	{
		// If we have a NamedSession, let the Session Interface figure this out for us, since both are listening.
		return;
	}

	// Cancel the current match ticket and re-advertise with the new number of open slots.
	// If the session isn't doing matchmaking, this is a no-op.
	SubmitMatchingTicket(MatchmakingTicket->GetLiveSessionRef(), SessionName, true);
}

//////////////////////////////////////////////////////////////////////////
FOnlineMatchTicketInfoPtr FOnlineMatchmakingInterfaceLive::GetMatchTicketForLiveSessionRef(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LiveSessionRef)
{
	FScopeLock ScopeLock( &TicketsLock );
	FOnlineMatchTicketInfoPtr FoundTicket;

	for (auto TicketPair : MatchmakingTickets)
	{
		FOnlineMatchTicketInfoPtr Ticket = TicketPair.Value;
		if (Ticket.IsValid() == false)
		{
			continue;
		}

		if (Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ TicketSessionRef = Ticket->GetLiveSessionRef())
		{
			if (TicketSessionRef &&
				FOnlineSubsystemLive::AreSessionReferencesEqual(TicketSessionRef, LiveSessionRef))
			{
				FoundTicket = Ticket;
				break;
			}
		}
	}

	return FoundTicket;
}
