// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "OnlineSessionSettings.h"

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

/**
 *  Async task base class for session related operations
 */
class FOnlineAsyncTaskLiveSessionBase : public FOnlineAsyncTaskLive
{
public:
	FOnlineAsyncTaskLiveSessionBase(
	class FOnlineSubsystemLive* LiveSubsystem,
		int32		InUserIndex,
		FName		SessionName,
		const FOnlineSessionSettings& NewSessionSettings
		);

	//. Copy constructor for new tasks based on old (e.g. Matchmaking advertisements)
	FOnlineAsyncTaskLiveSessionBase( FOnlineAsyncTaskLiveSessionBase* PreviousTask );

	virtual ~FOnlineAsyncTaskLiveSessionBase();

	virtual	bool					SettingsAreValid();

	const FName&					GetSessionName()	{ return SessionName; }

protected:
	void							ParseSessionSettings( FOnlineSessionSettings* InSessionSettings );
	void							ParseSearchSettings( const FOnlineSessionSearch* InSearchSettings );

protected:
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^				LiveSession;
	FOnlineSessionSettings			SessionSettings;
	FName							SessionName;
	FString							SessionTemplateName;
	uint32							SessionMaxSeats;

	FString							MatchHopperName;
	FString							MatchTicketAttributes;
	float							MatchTimeout;

	bool							ClientMatchmakingCapable;
	bool							PartyEnabledSession;
	Platform::String^				CustomConstantsJson;
	Windows::Foundation::Collections::IVector<Platform::String^>^		InitiatorXboxUserIds;
};


//------------------------------- End of file ---------------------------------
