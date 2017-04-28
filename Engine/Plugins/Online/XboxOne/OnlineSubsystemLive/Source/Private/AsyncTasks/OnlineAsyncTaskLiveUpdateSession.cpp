// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveUpdateSession.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"




FOnlineAsyncTaskLiveUpdateSession::FOnlineAsyncTaskLiveUpdateSession(
	FName InSessionName,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	int RetryCount,
	const FOnlineSessionSettings& InUpdatedSessionSettings)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InContext, InSubsystem, RetryCount)
	, UpdatedSessionSettings(InUpdatedSessionSettings)
{
	
}

FOnlineAsyncTaskLiveUpdateSession::FOnlineAsyncTaskLiveUpdateSession(
	FName InSessionName,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	int RetryCount,
	const FOnlineSessionSettings& InUpdatedSessionSettings)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InSessionReference, InContext, InSubsystem, RetryCount)
	, UpdatedSessionSettings(InUpdatedSessionSettings)
{
}

bool FOnlineAsyncTaskLiveUpdateSession::UpdateSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session)
{
	FOnlineSessionLive::WriteSettingsToLiveJson( UpdatedSessionSettings, Session, nullptr );
	// @ATG_CHANGE : BEGIN Allow modifying session visibility/joinability
	FOnlineSessionLive::WriteSessionPrivacySettingsToLiveJson(UpdatedSessionSettings, Session);
	// @ATG_CHANGE : END

	return true;
}


void FOnlineAsyncTaskLiveUpdateSession::TriggerDelegates()
{
	auto SessionInterface = Subsystem->GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->TriggerOnUpdateSessionCompleteDelegates(GetSessionName(), WasSuccessful());
	}
}