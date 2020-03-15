// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskLiveSafeWriteSession.h"

class FOnlineSessionLive;

/** 
 * Async item used to marshal a join request from the system callback thread to the game thread.
 */
class FOnlineAsyncTaskLiveUpdateSessionMember : public FOnlineAsyncTaskLiveSafeWriteSession
{
public:
	/**
	 * This constructor will look up the Live SessionReference by the InSessionName.
	 * Only call this from the game thread!
	 *
	 * @param InSessionName the name of the session to update
	 * @param InContext the Live context of the user who's updating the session
	 * @param InSubsystem the owning Live online subsystem
	 * @param RetryCount number of times to retry the session write
	 */
	FOnlineAsyncTaskLiveUpdateSessionMember(
			FName InSessionName,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			FOnlineSubsystemLive* InSubsystem,
			int RetryCount);

	/**
	 * This constructor takes the Live SessionReference directly and is safe to call
	 * in non-game threads.
	 *
	 * @param InSessionReference the name of the session to update
	 * @param InContext the Live context of the user who's updating the session
	 * @param InSubsystem the owning Live online subsystem
	 * @param RetryCount number of times to retry the session write
	 */
	FOnlineAsyncTaskLiveUpdateSessionMember(
			FName InSessionName,
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			FOnlineSubsystemLive* InSubsystem,
			int RetryCount);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveUpdateSessionMember"); }

protected:

	virtual bool UpdateSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session) override;

	virtual void TriggerDelegates();

	FName SessionIdentifier;
};
