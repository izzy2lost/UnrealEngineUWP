// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveUpdateSession.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"

using Microsoft::Xbox::Services::Multiplayer::MultiplayerSession;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction;
using Microsoft::Xbox::Services::XboxLiveContext;

FOnlineAsyncTaskLiveUpdateSession::FOnlineAsyncTaskLiveUpdateSession(
	FName InSessionName,
	XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	int32 RetryCount,
	const FOnlineSessionSettings& InUpdatedSessionSettings
)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InContext, InSubsystem, RetryCount)
	, UpdatedSessionSettings(InUpdatedSessionSettings)
{
}

FOnlineAsyncTaskLiveUpdateSession::FOnlineAsyncTaskLiveUpdateSession(
	FName InSessionName,
	MultiplayerSessionReference^ InSessionReference,
	XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	int32 RetryCount,
	const FOnlineSessionSettings& InUpdatedSessionSettings
)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InSessionReference, InContext, InSubsystem, RetryCount)
	, UpdatedSessionSettings(InUpdatedSessionSettings)
{
}

bool FOnlineAsyncTaskLiveUpdateSession::UpdateSession(MultiplayerSession^ Session)
{
	FOnlineSessionLive::WriteSettingsToLiveJson(UpdatedSessionSettings, Session, nullptr);

	// Write new session Joinability and Readability
	MultiplayerSessionRestriction SessionRestriction = FOnlineSessionLive::GetLiveSessionRestrictionFromSettings(UpdatedSessionSettings);
	Session->SessionProperties->JoinRestriction = SessionRestriction;
	Session->SessionProperties->ReadRestriction = SessionRestriction;

	// TODO: there might be other good things to update here?

	return true;
}

void FOnlineAsyncTaskLiveUpdateSession::TriggerDelegates()
{
	IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->TriggerOnUpdateSessionCompleteDelegates(GetSessionName(), WasSuccessful());
	}
}
