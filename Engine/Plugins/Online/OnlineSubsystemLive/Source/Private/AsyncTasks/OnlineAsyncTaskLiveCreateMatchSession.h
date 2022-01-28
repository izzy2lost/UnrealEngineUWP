// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskLiveSessionBase.h"
#include "Interfaces/OnlineSessionInterface.h"

class FOnlineSubsystemLive;

//-----------------------------------------------------------------------------
// Task to encapsulate Start Matchmaking
//-----------------------------------------------------------------------------

class FOnlineAsyncTaskLiveCreateMatchSession : public FOnlineAsyncTaskLiveSessionBase
{
public:
	//. Regular Matchmaking should use this constructor
	FOnlineAsyncTaskLiveCreateMatchSession(
		FOnlineSubsystemLive* InLiveSubsystem,
		const TArray< TSharedRef<const FUniqueNetId> >& InSearchingUserIds,
		const FName InSessionName,
		const FOnlineSessionSettings& InSessionSettings,
		const TSharedRef<FOnlineSessionSearch>& InSearchSettings
		);

	virtual ~FOnlineAsyncTaskLiveCreateMatchSession();

	virtual void Initialize() override;

	virtual FString ToString() const override { return TEXT("CreateMatchSession"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	// Store array of users
	TArray< TSharedRef<const FUniqueNetId> > SearchingUserIds;

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ CurrentMatchSessionRef;

	Windows::Xbox::Networking::SecureDeviceAssociation^ Association;
	Windows::Xbox::System::User^ SearchingUser;
	TSharedPtr<FInternetAddr> HostAddr;

	// Used to track completion of tasks that add local users to match session
	bool bSessionCreated;
	volatile int32 NumOtherLocalPlayersToAdd;

	void OnAddLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result);
};

//------------------------------- End of file ---------------------------------
