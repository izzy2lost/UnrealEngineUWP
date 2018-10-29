// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskLiveSafeWriteSession.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "../OnlineSubsystemLiveTypes.h"

/** 
 * Async task used to have another local user Leave() a Live session.
 */
class FOnlineAsyncTaskLiveUnregisterLocalUser : public FOnlineAsyncTaskLiveSafeWriteSession
{
public:
	FOnlineAsyncTaskLiveUnregisterLocalUser(
			FName InSessionName,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			FOnlineSubsystemLive* InSubsystem,
			const FUniqueNetIdLive& InUserId,
			const FOnUnregisterLocalPlayerCompleteDelegate& InDelegate);
			
	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("UnregisterLocalUser"); }
	virtual void TriggerDelegates() override;

private:

	// FOnlineAsyncTaskLiveSafeWriteSession
	virtual bool UpdateSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session);

	FUniqueNetIdLive UserId;
	FOnUnregisterLocalPlayerCompleteDelegate Delegate;
};
