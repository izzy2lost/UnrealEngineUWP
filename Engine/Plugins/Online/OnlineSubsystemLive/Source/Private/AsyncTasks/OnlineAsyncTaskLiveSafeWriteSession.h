// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"

class FOnlineSessionLive;

/**
 * Async item used to marshal a join request from the system callback thread to the game thread.
 */
class FOnlineAsyncTaskLiveSafeWriteSession : public FOnlineAsyncTaskLive
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
	 * @param bIsMatchSession Whether to modify the match session or game session
	 */
	FOnlineAsyncTaskLiveSafeWriteSession(
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
	 * @param bIsMatchSession Whether to modify the match session or game session
	 */
	FOnlineAsyncTaskLiveSafeWriteSession(
			FName InSessionName,
			Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			FOnlineSubsystemLive* InSubsystem,
			int RetryCount);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("SafeWriteSession"); }
	virtual void Finalize() override;
	virtual void Initialize() override;

	static const int DefaultRetryCount = 5;

protected:
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetLatestLiveSession() const { return LiveSession; }
	FName GetSessionName() const { return SessionName; }
	bool GetDidUpdateSession() const { return bUpdatedSession; }
	Microsoft::Xbox::Services::XboxLiveContext^ GetLiveContext() const { return LiveContext; }
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ GetSessionReference() const { return SessionReference; }

private:
	void OnFailed();
	void Retry(bool bGetSession);
	void TryWriteSession();

	/**
	 * Overridden by subclasses to perform the update on the local copy of the session document.
	 *
	 * @param Session The session to update
	 * @return true if the session was updated, false if not
	 */
	virtual bool UpdateSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session) = 0;

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference;
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;

	// Saved values used in Finalize()
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession;
	FName SessionName;

	int RetryCount;

	/** Store whether a subclass modified the session, so it can be referred to in Finalize() */
	bool bUpdatedSession;
};
