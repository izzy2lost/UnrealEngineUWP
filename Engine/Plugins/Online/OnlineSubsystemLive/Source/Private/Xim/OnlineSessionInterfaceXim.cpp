#include "../OnlineSubsystemLivePrivatePCH.h"

#if USE_XIM

#include "OnlineSessionInterfaceXim.h"
#include "SocketSubsystemXim.h"
#include "SocketsXim.h"
#include "OnlineVoiceInterfaceXim.h"
#include "XimMessageRouter.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineFriendsInterfaceLive.h"

#include <xboxintegratedmultiplayer.h>

//#include <xboxintegratedmultiplayer_c.h>
//#include <xboxintegratedmultiplayerimpl.h>

using namespace xbox::services::xbox_integrated_multiplayer;

namespace XimWellKnownPropertyNames
{
	static const FString ConnectionStringProperty = TEXT("ConnectionString");
	static const FString MultiplayerCorrelationIdProperty = TEXT("CorrelationId");
}

FOnlineSessionXim::FOnlineSessionXim(class FOnlineSubsystemLive* InSubsystem)
	: LiveSubsystem(InSubsystem)
{
}

FNamedOnlineSession* FOnlineSessionXim::AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings)
{
	return nullptr;
}

FNamedOnlineSession* FOnlineSessionXim::AddNamedSession(FName SessionName, const FOnlineSession& Session)
{
	return nullptr;
}

int32 SetAllowedJoinsWorkaroundCountdown;

void FOnlineSessionXim::Tick(float DeltaTime)
{
	CheckPendingSessionInvite();

	// This is the workaround for not being able to set join restrictions immediately (see OnXimMoveSucceeded)
	if (CurrentSession.IsValid() && CurrentSession->bHosting)
	{
		if (xim::singleton_instance().allowed_player_joins() == xim_allowed_player_joins::none)
		{
			if (SetAllowedJoinsWorkaroundCountdown <= 0)
			{
				switch (CurrentSession->SessionState)
				{
				case EOnlineSessionState::Pending:
				case EOnlineSessionState::Starting:
				case EOnlineSessionState::InProgress:
				case EOnlineSessionState::Ending:
				case EOnlineSessionState::Ended:
					SetXimAllowedJoinsFromSettings(CurrentSession->SessionSettings);
					break;

				default:
					break;
				}
			}
			else
			{
				--SetAllowedJoinsWorkaroundCountdown;
			}
		}
	}
}

FNamedOnlineSession* FOnlineSessionXim::GetNamedSession(FName SessionName)
{
	if (CurrentSession.IsValid() && SessionName == CurrentSession->SessionName)
	{
		return CurrentSession.Get();
	}
	return nullptr;
}

void FOnlineSessionXim::RemoveNamedSession(FName SessionName)
{
	if (CurrentSession.IsValid() && SessionName == CurrentSession->SessionName)
	{
		CurrentSession.Reset();
	}
}

EOnlineSessionState::Type FOnlineSessionXim::GetSessionState(FName SessionName) const
{
	if (CurrentSession.IsValid() && SessionName == CurrentSession->SessionName)
	{
		return CurrentSession->SessionState;
	}
	return EOnlineSessionState::NoSession;
}

bool FOnlineSessionXim::CreateSession(int32 HostingPlayerControllerIndex, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(HostingPlayerControllerIndex);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogOnline, Warning, L"Couldn't find unique id for HostingPlayerNum %d", HostingPlayerControllerIndex);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	return CreateSession(*UniqueId, SessionName, NewSessionSettings);
}

bool FOnlineSessionXim::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	if (CurrentSession.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("A local user (%s) is already in a XIM session.  Multiple XIM sessions on a single device are not supported."), *CurrentSession->LocalOwnerId->ToString());
		return false;
	}

	CurrentSession = MakeShareable(new FNamedOnlineSession(SessionName, NewSessionSettings));
	CurrentSession->SessionState = EOnlineSessionState::Creating;
	CurrentSession->bHosting = true;
	TSharedRef<FUniqueNetIdLive> OwningUserId = MakeShareable(new FUniqueNetIdLive(HostingPlayerId));
	CurrentSession->LocalOwnerId = OwningUserId;
	CurrentSession->SessionInfo = MakeShareable(new FOnlineSessionInfoXim(OwningUserId, false));
	auto XboxUserIdString = reinterpret_cast<PCWSTR>(HostingPlayerId.GetBytes());
	xim::singleton_instance().set_intended_local_xbox_user_ids(1, &XboxUserIdString);
	xim::singleton_instance().move_to_new_network(NewSessionSettings.NumPublicConnections + NewSessionSettings.NumPrivateConnections, xim_players_to_move::bring_only_local_players);

	HookXimEvents();

	return true;
}

bool FOnlineSessionXim::StartSession(FName SessionName)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't start an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
		TriggerOnStartSessionCompleteDelegates(SessionName, false);
		return false;
	}

	// Can't start a match multiple times
	if (Session->SessionState != EOnlineSessionState::Pending &&
		Session->SessionState != EOnlineSessionState::Ended)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't start an online session (%s) in state %s"),
			*SessionName.ToString(),
			EOnlineSessionState::ToString(Session->SessionState));
		TriggerOnStartSessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionState = EOnlineSessionState::InProgress;

	// Generate a new RoundId for Xbox events.
	TSharedPtr<FOnlineSessionInfoXim> XimSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoXim>(Session->SessionInfo);
	if (XimSessionInfo.IsValid())
	{
		XimSessionInfo->NewRound();
	}

	TriggerOnStartSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionXim::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	auto NamedSession = GetNamedSession(SessionName);

	if (!NamedSession)
	{
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (bShouldRefreshOnlineData)
	{
		SetXimAllowedJoinsFromSettings(UpdatedSessionSettings);
		SetXimCustomPropertiesFromSettings(UpdatedSessionSettings);
	}

	NamedSession->SessionSettings = UpdatedSessionSettings;

	TriggerOnUpdateSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionXim::EndSession(FName SessionName)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't end an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
		TriggerOnEndSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (Session->SessionState != EOnlineSessionState::InProgress)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't end session (%s) in state %s"),
			*SessionName.ToString(),
			EOnlineSessionState::ToString(Session->SessionState));
		TriggerOnEndSessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionState = EOnlineSessionState::Ended;
	TriggerOnEndSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionXim::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (Session->SessionState == EOnlineSessionState::Destroying)
	{
		// Purposefully skip the delegate call as one should already be in flight
		UE_LOG_ONLINE(Warning, TEXT("Already in process of destroying session (%s)"), *SessionName.ToString());
		return false;
	}

	Session->SessionState = EOnlineSessionState::Destroying;
	xim::singleton_instance().leave_network();

	ExtraDestroySessionDelegate = CompletionDelegate;

	// Events should be hooked by this point
	check(XimNetworkExitedHandle.IsValid());

	return true;
}

bool FOnlineSessionXim::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		return false;
	}
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	FString XboxUserIdString = UniqueId.ToString();
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XboxUserIdString == XimPlayers[i]->xbox_user_id())
		{
			return true;
		}
	}

	return false;
}

bool FOnlineSessionXim::StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	if (LocalPlayers.Num() == 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("No users provided.  Cannot matchmake."));
		return false;
	}

	TArray<PCWSTR> XuidArray;
	for (int32 i = 0; i < LocalPlayers.Num(); ++i)
	{
		XuidArray.Add(reinterpret_cast<PCWSTR>(LocalPlayers[i]->GetBytes()));
	}
	xim::singleton_instance().set_intended_local_xbox_user_ids(XuidArray.Num(), XuidArray.GetData());

	xim_matchmaking_configuration XimMatchSettings;
	switch (NewSessionSettings.NumPublicConnections)
	{
	case 2:
		XimMatchSettings.team_matchmaking_mode = xim_team_matchmaking_mode::no_teams_2_players_minimum_2;
		break;
	case 3:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_3_players_minimum_2 : xim_team_matchmaking_mode::no_teams_3_players_minimum_3;
		break;
	case 4:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_4_players_minimum_2 : xim_team_matchmaking_mode::no_teams_4_players_minimum_4;
		break;
	case 5:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_5_players_minimum_2 : xim_team_matchmaking_mode::no_teams_5_players_minimum_5;
		break;
	case 6:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_6_players_minimum_2 : xim_team_matchmaking_mode::no_teams_6_players_minimum_6;
		break;
	case 8:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_8_players_minimum_2 : xim_team_matchmaking_mode::no_teams_8_players_minimum_8;
		break;
	case 10:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_10_players_minimum_2 : xim_team_matchmaking_mode::no_teams_10_players_minimum_10;
		break;
	case 12:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_12_players_minimum_2 : xim_team_matchmaking_mode::no_teams_12_players_minimum_12;
		break;
	case 16:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_16_players_minimum_2 : xim_team_matchmaking_mode::no_teams_16_players_minimum_16;
		break;
	case 20:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_20_players_minimum_2 : xim_team_matchmaking_mode::no_teams_20_players_minimum_20;
		break;
	case 32:
		XimMatchSettings.team_matchmaking_mode = NewSessionSettings.bAllowJoinInProgress ? xim_team_matchmaking_mode::no_teams_32_players_minimum_2 : xim_team_matchmaking_mode::no_teams_32_players_minimum_32;
		break;

	default:
		UE_LOG_ONLINE(Warning, TEXT("Public connection count %d is not supported for XIM matchmaking"), NewSessionSettings.NumPublicConnections);
		return false;
	}

	FString GameModeString;
	if (SearchSettings->QuerySettings.Get(SETTING_GAMEMODE, GameModeString))
	{
		XimMatchSettings.custom_game_mode = FCrc::StrCrc32(*GameModeString);
	}
	else
	{
		XimMatchSettings.custom_game_mode = 0;
	}

	// These are not yet used by UE integration.
	XimMatchSettings.dlc_modes = 0;
	XimMatchSettings.required_roles = 0;
	XimMatchSettings.require_player_matchmaking_configuration = false;

	FNamedOnlineSession *ExistingSession = GetNamedSession(SessionName);
	if (ExistingSession)
	{
		// Backfill scenario.
		// Verify that the social session that we're going to be filling out is in a reasonable state.
		switch (ExistingSession->SessionState)
		{
		case EOnlineSessionState::Pending:
		case EOnlineSessionState::Starting:
		case EOnlineSessionState::InProgress:
		case EOnlineSessionState::Ending:
		case EOnlineSessionState::Ended:
			// Ok!
			break;

		case EOnlineSessionState::Creating:
		case EOnlineSessionState::NoSession:
		case EOnlineSessionState::Destroying:
		default:
			UE_LOG_ONLINE(Warning, TEXT("A local user (%s) is already in a XIM session, but backfill is not possible because the session is in state %d."), *CurrentSession->LocalOwnerId->ToString(), static_cast<int32>(CurrentSession->SessionState));
			return false;
		}

		xim::singleton_instance().set_backfill_matchmaking_configuration(&XimMatchSettings);
	}
	else
	{
		xim_players_to_move XimMoveType = xim_players_to_move::bring_only_local_players;
		if (CurrentSession.IsValid())
		{
			// We're already in a session, but it's different to the one we're requesting matchmaking for.
			// Scenario here is that social players group up in a 'lobby session' and then matchmake to fill
			// out the session for gameplay.  
			XimMoveType = xim_players_to_move::bring_existing_social_players;
		}

		// New session via matchmaking

		CurrentSession = MakeShareable(new FNamedOnlineSession(SessionName, NewSessionSettings));
		CurrentSession->SessionState = EOnlineSessionState::Creating;
		CurrentSession->bHosting = false;
		TSharedRef<const FUniqueNetIdLive> OwningUserId = StaticCastSharedRef<const FUniqueNetIdLive>(LocalPlayers[0]);
		CurrentSession->LocalOwnerId = OwningUserId;
		CurrentSession->SessionInfo = MakeShareable(new FOnlineSessionInfoXim(OwningUserId, true));
		xim::singleton_instance().move_to_network_using_matchmaking(XimMatchSettings, XimMoveType);
	}

	HookXimEvents();

	return true;
}

bool FOnlineSessionXim::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(SearchingPlayerNum);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogOnline, Warning, L"Couldn't find unique id for SearchingPlayerNum %d", SearchingPlayerNum);
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	return CancelMatchmaking(*UniqueId, SessionName);
}

bool FOnlineSessionXim::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
	FNamedOnlineSession *ExistingSession = GetNamedSession(SessionName);
	if (ExistingSession == nullptr)
	{
		UE_LOG(LogOnline, Warning, L"Couldn't find session %s", *SessionName.ToString());
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	switch (ExistingSession->SessionState)
	{
	case EOnlineSessionState::Pending:
	case EOnlineSessionState::Starting:
	case EOnlineSessionState::InProgress:
	case EOnlineSessionState::Ending:
	case EOnlineSessionState::Ended:
		// Here, 'CancelMatchmaking' means 'stop trying to backfill'
		xim::singleton_instance().set_backfill_matchmaking_configuration(nullptr);

		// Currently we invoke any callbacks inline.  We could alternatively move it to
		// a handler for backfill_matchmaking_configuration_changed, but we'd then need
		// to filter to the device that actually called CancelMatchmaking (everybody in
		// the network gets the XIM event).  Since either way it's possible to get join 
		// events after the cancel has supposedly completed I think it's simpler just to
		// leave it here.
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, true);
		return true;

	case EOnlineSessionState::Creating:
		// Here, 'CancelMatchmaking' means 'abandon an attempt to enter a new session via matchmaking'
		
		// Note: user parameters to StartMatchmaking and CancelMatchmaking don't quite line up (array of users vs single user).
		// Currently when one user abandons matchmaking, all local users do.  

		// Note that if remote players are involved in the matchmaking operation, it is NOT canceled for them.
		xim::singleton_instance().leave_network();
		return true;

	case EOnlineSessionState::NoSession:
	case EOnlineSessionState::Destroying:
	default:
		UE_LOG(LogOnline, Warning, L"CancelMatchmaking not supported for session %s is in state %d", *SessionName.ToString(), static_cast<int32>(ExistingSession->SessionState));
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	return false;
}

bool FOnlineSessionXim::FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	TSharedPtr<const FUniqueNetId> SearchingPlayerId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(SearchingPlayerNum);
	if (!SearchingPlayerId.IsValid())
	{
		SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	return FindSessions(SearchingPlayerNum, *SearchingPlayerId, SearchSettings);
}

bool FOnlineSessionXim::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	int32 SearchingPlayerNum = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(SearchingPlayerId);
	if (SearchingPlayerNum == -1)
	{
		SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	return FindSessions(SearchingPlayerNum, SearchingPlayerId, SearchSettings);
}

bool FOnlineSessionXim::FindSessions(int32 SearchingPlayerNum, const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	// Free up previous results
	SearchSettings->SearchResults.Empty();

	TSharedPtr<FOnlineSessionXim, ESPMode::ThreadSafe> SharedThis = AsShared();
	auto OnXimJoinableUsersCompleted = [SharedThis, SearchingPlayerNum, SearchSettings](const TArray<TSharedRef<FUniqueNetIdLive>>& JoinableUsers)
	{
		for (int i = 0; i < JoinableUsers.Num(); ++i)
		{
			FOnlineSessionSearchResult FoundSession;
			SharedThis->SessionSearchResultsFromUserId(SearchingPlayerNum, JoinableUsers[i], FoundSession);
			SearchSettings->SearchResults.Add(FoundSession);
		}

		SearchSettings->SearchState = EOnlineAsyncTaskState::Done;
		SharedThis->TriggerOnFindSessionsCompleteDelegates(true);
		SharedThis->LiveSubsystem->GetXimMessageRouter()->ClearOnXimJoinableUsersCompletedDelegate_Handle(SharedThis->XimJoinableUsersCompletedHandle);
	};

	if (!FindSessions(SearchingPlayerNum, SearchingPlayerId, FOnXimJoinableUsersCompletedDelegate::CreateLambda(OnXimJoinableUsersCompleted)))
	{
		SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
	return true;
}

bool FOnlineSessionXim::FindSessions(int32 SearchingPlayerNum, const FUniqueNetId& SearchingPlayerId, FOnXimJoinableUsersCompletedDelegate OnCompletionDelegate)
{
	if (XimJoinableUsersCompletedHandle.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("A search (FindSessions or FindFriendSession) is already in progress.  Multiple overlapping searches are not supported."));
		return false;
	}

	// Only social sessions are browseable in XIM, so let's go find those...

	// Kick off a read of the online friends list because if available it will be useful
	// for resolving xuids.
	IOnlineFriendsPtr FriendsInt = LiveSubsystem->GetFriendsInterface();
	if (FriendsInt.IsValid())
	{
		FriendsInt->ReadFriendsList(SearchingPlayerNum, ToString(EFriendsLists::OnlinePlayers));
	}

	auto LiveContext = LiveSubsystem->GetLiveContext(SearchingPlayerNum);
	if (LiveContext == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Unexpected error locating Live context for user %s."), *SearchingPlayerId.ToString());
		return false;
	}

	if (CurrentSession.IsValid())
	{
		bool FoundUser = false;
		uint32_t XimPlayerCount;
		xim_player_array XimPlayers;
		xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
		FString XboxUserIdString = SearchingPlayerId.ToString();
		for (uint32_t i = 0; i < XimPlayerCount; ++i)
		{
			if (XboxUserIdString == XimPlayers[i]->xbox_user_id())
			{
				FoundUser = true;
				break;
			}
		}

		if (!FoundUser)
		{
			UE_LOG_ONLINE(Warning, TEXT("When searching during a session the searching user must be a member of the current session."));
			return false;
		}
	}
	else
	{
		auto XboxUserIdString = reinterpret_cast<PCWSTR>(SearchingPlayerId.GetBytes());
		xim::singleton_instance().set_intended_local_xbox_user_ids(1, &XboxUserIdString);
	}

	XimJoinableUsersCompletedHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimJoinableUsersCompletedDelegate_Handle(OnCompletionDelegate);
	xim::singleton_instance().request_joinable_xbox_user_ids(xim_social_group::people);
	return true;
}


void FOnlineSessionXim::SessionSearchResultsFromUserId(int32 LocalUserNum, TSharedRef<FUniqueNetIdLive> SessionOwnerId, FOnlineSessionSearchResult& OutSearchResult)
{
	OutSearchResult.Session.NumOpenPrivateConnections = 0;
	OutSearchResult.Session.NumOpenPublicConnections = 0;
	OutSearchResult.Session.SessionSettings.NumPublicConnections = 0;
	OutSearchResult.Session.OwningUserId = SessionOwnerId;
	OutSearchResult.Session.SessionInfo = MakeShareable(new FOnlineSessionInfoXim(SessionOwnerId, false));

	// With any luck the friends read that we kicked before calling GetActivitiesXXX has results by now and we'll
	// be able to report the session with a friendly user name.
	// Consider whether we should report failure if a proper name isn't available?
	TSharedPtr<FOnlineFriend> Friend = LiveSubsystem->GetFriendsInterface()->GetFriend(LocalUserNum, *SessionOwnerId, ToString(EFriendsLists::OnlinePlayers));
	if (Friend.IsValid())
	{
		OutSearchResult.Session.OwningUserName = Friend->GetDisplayName();
	}
}

bool FOnlineSessionXim::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate)
{
	return false;
}

bool FOnlineSessionXim::JoinSession(int32 ControllerIndex, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(ControllerIndex);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogOnline, Log, L"Couldn't find unique id for HostingPlayerNum %d", ControllerIndex);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	return JoinSession(*UniqueId, SessionName, DesiredSession);
}

bool FOnlineSessionXim::JoinSession(const FUniqueNetId& UserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	if (CurrentSession.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("A local user (%s) is already in a XIM session.  Multiple XIM sessions on a single device are not supported."), *CurrentSession->LocalOwnerId->ToString());
		return false;
	}

	CurrentSession = MakeShareable(new FNamedOnlineSession(SessionName, DesiredSession.Session));
	CurrentSession->LocalOwnerId = MakeShareable(new FUniqueNetIdLive(UserId));
	CurrentSession->SessionState = EOnlineSessionState::Creating;
	CurrentSession->bHosting = false;
	auto XboxUserIdString = reinterpret_cast<PCWSTR>(UserId.GetBytes());
	xim::singleton_instance().set_intended_local_xbox_user_ids(1, &XboxUserIdString);

	TSharedPtr<FOnlineSessionInfoXim> XimSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoXim>(DesiredSession.Session.SessionInfo);
	if (XimSessionInfo->GetProtocolActivationString().IsEmpty())
	{
		xim::singleton_instance().move_to_network_using_joinable_xbox_user_id(*DesiredSession.Session.OwningUserId->ToString());
	}
	else
	{
		xim::singleton_instance().move_to_network_using_protocol_activated_event_args(*XimSessionInfo->GetProtocolActivationString());
	}

	HookXimEvents();

	return true;
}

bool FOnlineSessionXim::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
	TSharedPtr<const FUniqueNetId> LocalUserId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum);
	if (!LocalUserId.IsValid())
	{
		TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, TArray<FOnlineSessionSearchResult>());
		return false;
	}

	TArray<TSharedRef<const FUniqueNetId>> FriendList;
	FriendList.Emplace(MakeShared<FUniqueNetIdLive>(static_cast<const FUniqueNetIdLive&>(Friend)));
	return FindFriendSession(LocalUserNum, *LocalUserId, FriendList);
}

bool FOnlineSessionXim::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList)
{
	TSharedPtr<FOnlineSessionXim, ESPMode::ThreadSafe> SharedThis = AsShared();
	auto OnXimJoinableUsersCompleted = [SharedThis, FriendList, LocalUserNum](const TArray<TSharedRef<FUniqueNetIdLive>>& JoinableUsers)
	{
		bool FoundSession = false;
		TArray<FOnlineSessionSearchResult> FriendSessions;
		for (int i = 0; i < JoinableUsers.Num(); ++i)
		{
			for (int j = 0; j < FriendList.Num(); ++j)
			{
				TSharedRef<const FUniqueNetIdLive> FriendIdLive = StaticCastSharedRef<const FUniqueNetIdLive>(FriendList[j]);
				if (*JoinableUsers[i] == *FriendIdLive)
				{
					FOnlineSessionSearchResult SearchResult;
					SharedThis->SessionSearchResultsFromUserId(LocalUserNum, JoinableUsers[i], SearchResult);
					FriendSessions.Add(SearchResult);
					FoundSession = true;
				}
			}

		}

		SharedThis->TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, FoundSession, FriendSessions);
		SharedThis->LiveSubsystem->GetXimMessageRouter()->ClearOnXimJoinableUsersCompletedDelegate_Handle(SharedThis->XimJoinableUsersCompletedHandle);
	};

	if (!FindSessions(LocalUserNum, LocalUserId, FOnXimJoinableUsersCompletedDelegate::CreateLambda(OnXimJoinableUsersCompleted)))
	{
		TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, TArray<FOnlineSessionSearchResult>());
	}

	return true;
}

bool FOnlineSessionXim::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	int32 LocalUserNum = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(LocalUserId);
	if (LocalUserNum == -1)
	{
		TriggerOnFindFriendSessionCompleteDelegates(-1, false, TArray<FOnlineSessionSearchResult>());
		return false;
	}

	TArray<TSharedRef<const FUniqueNetId>> FriendList;
	FriendList.Emplace(MakeShared<FUniqueNetIdLive>(static_cast<const FUniqueNetIdLive&>(Friend)));
	return FindFriendSession(LocalUserNum, LocalUserId, FriendList);
}

bool FOnlineSessionXim::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList)
{
	int32 LocalUserNum = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(LocalUserId);
	if (LocalUserNum == -1)
	{
		TriggerOnFindFriendSessionCompleteDelegates(-1, false, TArray<FOnlineSessionSearchResult>());
		return false;
	}
	return FindFriendSession(LocalUserNum, LocalUserId, FriendList);
}

bool FOnlineSessionXim::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogOnline, Log, L"Couldn't find unique id for LocalUserNum %d", LocalUserNum);
		return false;
	}

	return SendSessionInviteToFriend(*UniqueId, SessionName, Friend);
}

bool FOnlineSessionXim::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	TArray<TSharedRef<const FUniqueNetId>> FriendsArray;
	FriendsArray.Init(MakeShareable(new FUniqueNetIdLive(Friend)), 1);
	return SendSessionInviteToFriends(LocalUserId, SessionName, FriendsArray);

}

bool FOnlineSessionXim::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	auto UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogOnline, Log, L"Couldn't find unique id for LocalUserNum %d", LocalUserNum);
		return false;
	}
	return SendSessionInviteToFriends(*UniqueId, SessionName, Friends);
}

bool FOnlineSessionXim::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	FString XboxUserIdString = LocalUserId.ToString();
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XboxUserIdString == XimPlayers[i]->xbox_user_id())
		{
			xim_player::xim_local* XimLocal = XimPlayers[i]->local();
			if (XimLocal != nullptr)
			{
				TArray<PCWSTR> XuidArray;
				XuidArray.SetNumUninitialized(Friends.Num());
				for (int32 i = 0; i < Friends.Num(); ++i)
				{
					XuidArray[i] = reinterpret_cast<PCWSTR>(Friends[i]->GetBytes());
				}
				XimLocal->invite_users(XuidArray.Num(), XuidArray.GetData(), nullptr);
				return true;
			}
		}
	}
	return false;
}

bool FOnlineSessionXim::GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		return false;
	}

	ConnectInfo = xim::singleton_instance().get_network_custom_property(*XimWellKnownPropertyNames::ConnectionStringProperty);
	return !ConnectInfo.IsEmpty();
}

bool FOnlineSessionXim::GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	// Connection info comes from a network property, and we currently can't access properties for networks until we've joined them.

	return false;
}

FOnlineSessionSettings* FOnlineSessionXim::GetSessionSettings(FName SessionName)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		return nullptr;
	}
	return &Session->SessionSettings;
}

bool FOnlineSessionXim::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdLive(PlayerId)));
	return RegisterPlayers(SessionName, Players, bWasInvited);
}

bool FOnlineSessionXim::RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited)
{
	bool bSuccess = false;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		if (Session->SessionInfo.IsValid())
		{
			for (int32 PlayerIdx = 0; PlayerIdx < Players.Num(); PlayerIdx++)
			{
				const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];

				FUniqueNetIdMatcher PlayerMatch(*PlayerId);
				if (Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch) == INDEX_NONE)
				{
					Session->RegisteredPlayers.Add(PlayerId);
				}
				else
				{
					UE_LOG_ONLINE(Log, TEXT("Player %s already registered in session %s"), *Players[PlayerIdx]->ToDebugString(), *SessionName.ToString());
				}
			}

			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No session info to join for session (%s)"), *SessionName.ToString());
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No game present to join for session (%s)"), *SessionName.ToString());
	}

	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

bool FOnlineSessionXim::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdLive(PlayerId)));
	return UnregisterPlayers(SessionName, Players);
}

bool FOnlineSessionXim::UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players)
{
	bool bSuccess = false;

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		if (Session->SessionInfo.IsValid())
		{
			for (int32 PlayerIdx = 0; PlayerIdx < Players.Num(); PlayerIdx++)
			{
				const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];

				FUniqueNetIdMatcher PlayerMatch(*PlayerId);
				int32 RegistrantIndex = Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch);
				if (RegistrantIndex != INDEX_NONE)
				{
					Session->RegisteredPlayers.RemoveAtSwap(RegistrantIndex);
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("Player %s is not part of session (%s)"), *PlayerId->ToDebugString(), *SessionName.ToString());
				}
			}

			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No session info to leave for session (%s)"), *SessionName.ToString());
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No game present to leave for session (%s)"), *SessionName.ToString());
	}

	TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

void FOnlineSessionXim::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't register a local player for session (%s) that hasn't been created"), *SessionName.ToString());
		Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::SessionDoesNotExist);
		return;
	}

	// This is called for a split-screen join: the provided player should be added to XIM network.
	FString NewPlayerIdString = PlayerId.ToString();

	TArray<PCWSTR> XuidArray;
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XimPlayers[i]->local())
		{
			if (NewPlayerIdString != XimPlayers[i]->xbox_user_id())
			{
				XuidArray.Add(XimPlayers[i]->xbox_user_id());
			}
			else
			{
				UE_LOG(LogOnline, Warning, L"Player %s is already in the current XIM session.", *NewPlayerIdString);
				Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::AlreadyInSession);
				return;
			}
		}
	}

	XuidArray.Add(*NewPlayerIdString);
	xim::singleton_instance().set_intended_local_xbox_user_ids(XuidArray.Num(), XuidArray.GetData());

	LocalPlayerRegisteredDelegates.Add(FUniqueNetIdLive(PlayerId), Delegate);

	// Since someone local is already in the session (or else it wouldn't exist), so we should already have
	// events hooked appropriately.
	check(XimPlayerJoinedHandle.IsValid());
	check(XimPlayerLeftHandle.IsValid());
}

void FOnlineSessionXim::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	auto Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't unregister a local player for session (%s) that hasn't been created"), *SessionName.ToString());
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	FString LeavingPlayerIdString = PlayerId.ToString();
	bool FoundLeavingPlayer = false;

	TArray<PCWSTR> XuidArray;
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XimPlayers[i]->local())
		{
			if (LeavingPlayerIdString != XimPlayers[i]->xbox_user_id())
			{
				XuidArray.Add(XimPlayers[i]->xbox_user_id());
			}
			else
			{
				FoundLeavingPlayer = true;
			}
		}
	}

	if (!FoundLeavingPlayer)
	{
		UE_LOG(LogOnline, Warning, L"Player %s not found in the current XIM session.", *LeavingPlayerIdString);
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	xim::singleton_instance().set_intended_local_xbox_user_ids(XuidArray.Num(), XuidArray.GetData());

	LocalPlayerUnregisteredDelegates.Add(FUniqueNetIdLive(PlayerId), Delegate);
}

int32 FOnlineSessionXim::GetNumSessions()
{
	return CurrentSession.IsValid() ? 1 : 0;
}

bool FOnlineSessionXim::HasPresenceSession()
{
	return CurrentSession.IsValid();
}

void FOnlineSessionXim::CheckPendingSessionInvite()
{
	// If the title was protocol activated before OSS was initialized (likely at launch), the startup code saved the
	// activation URI.
	const FString ActivationUriString = FPlatformMisc::GetProtocolActivationUri();

	if (!ActivationUriString.IsEmpty())
	{
		xim_protocol_activation_information ActivationInfo;
		if (xim::extract_protocol_activation_information(*ActivationUriString, &ActivationInfo))
		{
			TSharedPtr<const FUniqueNetId> UniqueNetIdPtr = MakeShareable(new FUniqueNetIdLive(ActivationInfo.local_xbox_user_id));
			int32 AcceptingUserIndex = LiveSubsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(*UniqueNetIdPtr);
			FOnlineSessionSearchResult SearchResult;
			SearchResult.Session.NumOpenPrivateConnections = 0;
			SearchResult.Session.NumOpenPublicConnections = 1; // ??
			SearchResult.Session.SessionSettings.NumPublicConnections = 1; // ??
			TSharedRef<FUniqueNetIdLive> OwningUserId = MakeShareable(new FUniqueNetIdLive(ActivationInfo.target_xbox_user_id));
			SearchResult.Session.OwningUserId = OwningUserId;
			SearchResult.Session.SessionInfo = MakeShareable(new FOnlineSessionInfoXim(OwningUserId, ActivationUriString));

			TriggerOnSessionUserInviteAcceptedDelegates(true, AcceptingUserIndex, UniqueNetIdPtr, SearchResult);

			FPlatformMisc::SetProtocolActivationUri(FString());
		}
	}
}

void FOnlineSessionXim::SetXimAllowedJoinsFromSettings(const FOnlineSessionSettings& SessionSettings)
{
	// XIM's join restrictions don't align with UE's: in XIM join via presence is always friends-only,
	// and enabling it always implies that invites are allowed. 
	xim_allowed_player_joins AllowedJoinMode;
	if (SessionSettings.bAllowJoinViaPresence || SessionSettings.bAllowJoinViaPresenceFriendsOnly)
	{
		AllowedJoinMode = xim_allowed_player_joins::local_invited_or_followed;
	}
	else if (SessionSettings.bAllowInvites)
	{
		AllowedJoinMode = xim_allowed_player_joins::local_or_invited;
	}
	else
	{
		AllowedJoinMode = xim_allowed_player_joins::none;
	}

	xim::singleton_instance().set_allowed_player_joins(AllowedJoinMode);
}

void FOnlineSessionXim::SetXimCustomPropertiesFromSettings(const FOnlineSessionSettings& SessionSettings)
{
	for (FSessionSettings::TConstIterator It(SessionSettings.Settings); It; ++It)
	{
		const FName& SettingName = It.Key();
		const FOnlineSessionSetting& SettingValue = It.Value();

		// Only upload values that are marked for service use
		if (SettingValue.AdvertisementType >= EOnlineDataAdvertisementType::ViaOnlineService)
		{
			xim::singleton_instance().set_network_custom_property(*SettingName.ToString(), *SettingValue.Data.ToString());
		}
	}
}


void FOnlineSessionXim::OnXimPlayerJoined(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer)
{
	if (CurrentSession.IsValid())
	{
		FUniqueNetIdLive UniqueId(XimPlayer->xbox_user_id());
		FOnRegisterLocalPlayerCompleteDelegate* LocalRegistrationDelegate = LocalPlayerRegisteredDelegates.Find(UniqueId);
		if (LocalRegistrationDelegate != nullptr)
		{
			LocalRegistrationDelegate->ExecuteIfBound(UniqueId, EOnJoinSessionCompleteResult::Success);
			LocalPlayerRegisteredDelegates.Remove(UniqueId);
		}
	}
}

void FOnlineSessionXim::OnXimPlayerLeft(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer)
{
	FUniqueNetIdLive UniqueId(XimPlayer->xbox_user_id());
	FOnUnregisterLocalPlayerCompleteDelegate* LocalUnregistrationDelegate = LocalPlayerUnregisteredDelegates.Find(UniqueId);
	if (LocalUnregistrationDelegate != nullptr)
	{
		LocalUnregistrationDelegate->ExecuteIfBound(UniqueId, true);
		LocalPlayerUnregisteredDelegates.Remove(UniqueId);
	}
}


void FOnlineSessionXim::OnXimMoveSucceeded()
{
	if (!CurrentSession.IsValid())
	{
		return;
	}

	if (CurrentSession->SessionState != EOnlineSessionState::Creating)
	{
		// This ought to be a backfill scenario.
		// TODO: need to make sure allowed join property is set correctly on new session.
		return;
	}

	// Determine the identity of the UE game server based on a custom property
	// Do this first so late joiners can skip the remainder of the below.
	FString ExistingConnectionString;
	if (GetResolvedConnectString(CurrentSession->SessionName, ExistingConnectionString))
	{
		OnListenServerDetermined();
		return;
	}

	TSharedPtr<FOnlineSessionInfoXim> XimSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoXim>(CurrentSession->SessionInfo);
	if (!XimSessionInfo.IsValid())
	{
		return;
	}

	if (XimSessionInfo->IsMatchmade())
	{
		xim_authority* XimAuthority = xim::singleton_instance().authority();

		// This would probably be the best way to pick a host, but it's not yet implemented
		//CurrentSession->bHosting = XimAuthority->is_local();

		// As a temporary solution we'll declare the player with the lowest XUID string to be the host.
		uint32_t XimPlayerCount;
		xim_player_array XimPlayers;
		xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
		check(XimPlayerCount > 1);
		xim_player* SelectedHost = nullptr;
		for (uint32_t i = 0; i < XimPlayerCount; ++i)
		{
			if (!SelectedHost || wcscmp(XimPlayers[i]->xbox_user_id(), SelectedHost->xbox_user_id()) < 0)
			{
				SelectedHost = XimPlayers[i];
			}
		}
		check(SelectedHost);

		CurrentSession->bHosting = SelectedHost->local() != nullptr;
		CurrentSession->OwningUserId = MakeShareable(new FUniqueNetIdLive(SelectedHost->xbox_user_id()));
	}

	if (CurrentSession->bHosting)
	{
		// If we're the UE game host then this network is brand new, either via move_to_new_network or
		// matchmaking.  XIM will have set default join restrictions, which now should be modified to
		// match UE as much as possible. 
		xim_authority* XimAuthority = xim::singleton_instance().authority();

		// Authority should definitely be local at this point, since either only local players are in
		// the network or else we're matchmade and the host was deliberately selected to match the xim_authority.
		//check(XimAuthority->is_local());

		// Possibly it's important to wait for this to complete before triggering delegates?
		// Mostly in case we want to register additional local players (which would require
		// join mode not none).
		//SetXimAllowedJoinsFromSettings(CurrentSession->SessionSettings);

		// Currently changing the join restriction directly in response to the move will fail
		// because there's another change queued internally.  So we insert a delay (arbitrary, 
		// but enough to allow that change to work its way through) and then set the true value
		// on a later Tick
		SetAllowedJoinsWorkaroundCountdown = 10;
		
		FString ConnectionString = FString(TEXT("[")) + CurrentSession->LocalOwnerId->ToString() + TEXT("]");
		
		xim::singleton_instance().set_network_custom_property(*XimWellKnownPropertyNames::ConnectionStringProperty, *ConnectionString);

		// XIM titles are not required to supply a correlation ID that matches that supplied by the MPSD session.  In fact
		// the MPSD value is not accessible by design.  We generate our own value for consistency with the non-XIM code path.
		xim::singleton_instance().set_network_custom_property(*XimWellKnownPropertyNames::MultiplayerCorrelationIdProperty, *FGuid::NewGuid().ToString());

		SetXimCustomPropertiesFromSettings(CurrentSession->SessionSettings);
	}
}

void FOnlineSessionXim::OnXimNetworkExited(xbox::services::xbox_integrated_multiplayer::xim_network_exit_reason Reason)
{
	if (CurrentSession.IsValid())
	{
		TSharedPtr<FNamedOnlineSession> SessionBeingExited = CurrentSession;
		RemoveNamedSession(SessionBeingExited->SessionName);

		switch (SessionBeingExited->SessionState)
		{
		case EOnlineSessionState::Destroying:
			// Graceful departure
			ExtraDestroySessionDelegate.ExecuteIfBound(SessionBeingExited->SessionName, true);
			ExtraDestroySessionDelegate.Unbind();
			TriggerOnDestroySessionCompleteDelegates(SessionBeingExited->SessionName, true);
			break;

		case EOnlineSessionState::Creating:
			{
				// Failure during move_to network.
				UE_LOG_ONLINE(Warning, TEXT("XIM move call failed with reason %d."), static_cast<int32>(Reason));

				TSharedPtr<FOnlineSessionInfoXim> XimSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoXim>(SessionBeingExited->SessionInfo);
				if (XimSessionInfo.IsValid())
				{
					if (XimSessionInfo->IsMatchmade())
					{
						if (Reason == xim_network_exit_reason::called_leave_network)
						{
							// Explicit cancel
							TriggerOnCancelMatchmakingCompleteDelegates(SessionBeingExited->SessionName, true);
						}
						TriggerOnMatchmakingCompleteDelegates(SessionBeingExited->SessionName, false);
					}
					else if (SessionBeingExited->bHosting)
					{
						TriggerOnCreateSessionCompleteDelegates(SessionBeingExited->SessionName, false);
					}
					else
					{
						EOnJoinSessionCompleteResult::Type JoinResult;
						switch (Reason)
						{
						case xim_network_exit_reason::network_full:
							JoinResult = EOnJoinSessionCompleteResult::SessionIsFull;
							break;

						case xim_network_exit_reason::network_no_longer_exists:
							JoinResult = EOnJoinSessionCompleteResult::SessionDoesNotExist;
							break;

						default:
							JoinResult = EOnJoinSessionCompleteResult::UnknownError;
							break;
						}
						TriggerOnJoinSessionCompleteDelegates(SessionBeingExited->SessionName, JoinResult);
					}
				}
			}
			break;

		default:
			// Abnormal depature.  Should we be triggering delegates here?
			UE_LOG_ONLINE(Warning, TEXT("Left XIM network with session in unexpected state %d (reason %d)."), static_cast<int32>(SessionBeingExited->SessionState), static_cast<int32>(Reason));
			break;
		}
	}
}

void FOnlineSessionXim::OnListenServerDetermined()
{
	if (CurrentSession.IsValid())
	{
		if (CurrentSession->SessionState == EOnlineSessionState::Creating)
		{
			TSharedPtr<FOnlineSessionInfoXim> XimSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoXim>(CurrentSession->SessionInfo);
			if (XimSessionInfo.IsValid())
			{
				CurrentSession->SessionState = EOnlineSessionState::Pending;
				if (XimSessionInfo->IsMatchmade())
				{
					TriggerOnMatchmakingCompleteDelegates(CurrentSession->SessionName, true);
				}
				else if (CurrentSession->bHosting)
				{
					TriggerOnCreateSessionCompleteDelegates(CurrentSession->SessionName, true);
				}
				else
				{
					TriggerOnJoinSessionCompleteDelegates(CurrentSession->SessionName, EOnJoinSessionCompleteResult::Success);
				}
			}
		}
	}
}

void FOnlineSessionXim::OnXimPlayerPropertyChanged(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer, const FString& PropertyName)
{
	// Consider a property update mechanism where UE game server writes session properties as player properties 
	// and XIM authority collects changes and applies them as network properties.
}

void FOnlineSessionXim::OnXimNetworkPropertyChanged(const FString& PropertyName)
{
	if (PropertyName == XimWellKnownPropertyNames::ConnectionStringProperty)
	{
		OnListenServerDetermined();
	}
	else if (PropertyName == XimWellKnownPropertyNames::MultiplayerCorrelationIdProperty)
	{
		TSharedPtr<FOnlineSessionInfoXim> XimSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoXim>(CurrentSession->SessionInfo);
		if (XimSessionInfo.IsValid())
		{
			XimSessionInfo->SetMultiplayerCorrelationId(xim::singleton_instance().get_network_custom_property(*PropertyName));
		}
	}
	else if (CurrentSession.IsValid())
	{
		FString PropertyValue = xim::singleton_instance().get_network_custom_property(*PropertyName);
		CurrentSession->SessionSettings.Settings.FindOrAdd(FName(*PropertyName)) = FOnlineSessionSetting(PropertyValue, EOnlineDataAdvertisementType::ViaOnlineService);
	}
}

void FOnlineSessionXim::HookXimEvents()
{
	if (!XimPlayerJoinedHandle.IsValid())
	{
		XimPlayerJoinedHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimPlayerJoinedDelegate_Handle(FOnXimPlayerJoinedDelegate::CreateThreadSafeSP(this, &FOnlineSessionXim::OnXimPlayerJoined));
	}
	if (!XimPlayerLeftHandle.IsValid())
	{
		XimPlayerLeftHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimPlayerLeftDelegate_Handle(FOnXimPlayerLeftDelegate::CreateThreadSafeSP(this, &FOnlineSessionXim::OnXimPlayerLeft));
	}
	if (!XimMoveSucceededHandle.IsValid())
	{
		XimMoveSucceededHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimMoveSucceededDelegate_Handle(FOnXimMoveSucceededDelegate::CreateThreadSafeSP(this, &FOnlineSessionXim::OnXimMoveSucceeded));
	}
	if (!XimNetworkExitedHandle.IsValid())
	{
		XimNetworkExitedHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimNetworkExitedDelegate_Handle(FOnXimNetworkExitedDelegate::CreateThreadSafeSP(this, &FOnlineSessionXim::OnXimNetworkExited));
	}
	if (!XimPlayerPropertyChangedHandle.IsValid())
	{
		XimPlayerPropertyChangedHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimPlayerPropertyChangedDelegate_Handle(FOnXimPlayerPropertyChangedDelegate::CreateThreadSafeSP(this, &FOnlineSessionXim::OnXimPlayerPropertyChanged));
	}
	if (!XimNetworkPropertyChangedHandle.IsValid())
	{
		XimNetworkPropertyChangedHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimNetworkPropertyChangedDelegate_Handle(FOnXimNetworkPropertyChangedDelegate::CreateThreadSafeSP(this, &FOnlineSessionXim::OnXimNetworkPropertyChanged));
	}
}

#endif