// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveCreateSession.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "OnlineSubsystemLivePackage.h"
#include "Online.h"
// @ATG_CHANGE : BEGIN - Allow modifying session visibility/joinability
#include "../OnlineIdentityInterfaceLive.h"
// @ATG_CHANGE : END

#include <collection.h>

using namespace Platform;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;

FOnlineAsyncTaskLiveCreateSession::FOnlineAsyncTaskLiveCreateSession(
	FOnlineSessionLive* InSessionInterface,
	TSharedPtr<const FUniqueNetId> InUserId,
	FName InSessionName, 
	MultiplayerSession^ InLiveSession)
	: SessionInterface(InSessionInterface)
	, UserId(InUserId)
	, SessionName(InSessionName)
	, LiveSession(InLiveSession)
	, bWasSuccessful(true)
{
	check(SessionInterface);
}

void FOnlineAsyncTaskLiveCreateSession::Finalize()
{
	// Find the named session and link LIVE platform data to it
	auto NamedSession = SessionInterface->GetNamedSession(SessionName);
	
	if (NamedSession == nullptr)
	{
		bWasSuccessful = false;
		return;
	}

	NamedSession->HostingPlayerNum = INDEX_NONE;
	NamedSession->OwningUserId = UserId;

	FOnlineSessionInfoLivePtr NewSessionInfo = MakeShared<FOnlineSessionInfoLive>(LiveSession);
	NewSessionInfo->SetSessionReady();

	NamedSession->SessionInfo  = NewSessionInfo;
	NamedSession->SessionState = EOnlineSessionState::Pending;
	
	FOnlineSessionLive::ReadSettingsFromLiveJson( LiveSession, *NamedSession );
	
	// Initialize session state after create/join
	FOnlineSubsystemLive* Subsystem = static_cast<FOnlineSubsystemLive*>(IOnlineSubsystem::Get(LIVE_SUBSYSTEM));
	if (!Subsystem)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveCreateSession::Finalize - Couldn't get Live subsystem"));
	}

	// @ATG_CHANGE : BEGIN - Allow modifying session visibility/joinability
	NamedSession->HostingPlayerNum = Subsystem->GetIdentityLive()->GetPlatformUserIdFromUniqueNetId(*UserId);
	// @ATG_CHANGE : END

	Subsystem->GetSessionMessageRouter()->SyncInitialSessionState(SessionName, LiveSession);
}

void FOnlineAsyncTaskLiveCreateSession::TriggerDelegates() 
{
	SessionInterface->TriggerOnCreateSessionCompleteDelegates(SessionName, bWasSuccessful);
}
