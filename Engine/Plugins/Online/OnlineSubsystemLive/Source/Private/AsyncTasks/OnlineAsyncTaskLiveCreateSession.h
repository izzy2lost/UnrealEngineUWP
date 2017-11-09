// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"

class FOnlineSessionLive;
class FOnlineSessionSettings;

/** 
 *  Async task for creating a LIVE session
 */
class FOnlineAsyncTaskLiveCreateSession : public FOnlineAsyncItem
{
private:
	/** The session that created this task */
	FOnlineSessionLive* SessionInterface;

	/** The id of the user hosting the session */
	TSharedPtr<const FUniqueNetId> UserId;

	/** The name of the session */
	FName SessionName;

	/** The updated Live session after the write operation */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession;

	/** Whether the operation was successful */
	bool bWasSuccessful;

public:
	/**
	 * Constructor
	 *
	 * @param InSessionInterface The session that created this task
	 * @param InUserIndex Index of the user who created this task
	 * @param InCreator User who created this task
	 * @param InSessionName Name of the session to create
	 */
	FOnlineAsyncTaskLiveCreateSession(
			FOnlineSessionLive* InSessionInterface,
			TSharedPtr<const FUniqueNetId> InUserId,
			FName InSessionName, 
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ InLiveSession);
	
	// FOnlineAsyncItem
	virtual FString ToString() const override { return FString::Printf(TEXT("FOnlineAsyncTaskLiveCreateSession SessionName: %s bWasSuccessful: %d"), *SessionName.ToString(), bWasSuccessful); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;
};
