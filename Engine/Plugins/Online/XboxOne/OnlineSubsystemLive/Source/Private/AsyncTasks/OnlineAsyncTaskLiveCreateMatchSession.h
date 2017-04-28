// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskLiveSessionBase.h"
#include "OnlineSessionInterface.h"

//-----------------------------------------------------------------------------
// Task to encapsulate Start Matchmaking
//-----------------------------------------------------------------------------

class FOnlineAsyncTaskLiveCreateMatchSession : public FOnlineAsyncTaskLiveSessionBase
{
public:
	//. Regular Matchmaking should use this constructor
	FOnlineAsyncTaskLiveCreateMatchSession(
		class FOnlineSubsystemLive* InLiveSubsystem,
		const TArray< TSharedRef<const FUniqueNetId> >& InSearchingUserIds,
		FName InSessionName,
		const FOnlineSessionSettings& InSessionSettings,
		TSharedPtr<FOnlineSessionSearch>& InSearchSettings
		);

	virtual ~FOnlineAsyncTaskLiveCreateMatchSession();

	virtual void Initialize() override;

	virtual FString ToString() const override { return TEXT("CreateMatchSession"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	// Store array of users
	TArray< TSharedRef<const FUniqueNetId> > SearchingUserIds;

	TSharedPtr<FOnlineSessionSearch> SearchSettings;

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
