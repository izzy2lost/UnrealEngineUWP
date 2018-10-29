// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineAsyncTaskLiveGetLeaderboard.h"
#include "OnlineStats.h"

using namespace Microsoft::Xbox::Services::Leaderboard;

/**
 *	Async task to retrieve a leaderboard containing a list of users from Live
 */
class FOnlineAsyncTaskLiveGetLeaderboardForUsers : public FOnlineAsyncTaskBasic<FOnlineSubsystemLive>
{
public:
	/**
	 * Constructor
	 *
	 * @param InLiveSubsystem - The LiveOnlineSubsystem used to retrieve the leaderboards interface
	 * @param InLeaderboardReads - The array of LeaderboardReads to consolidate into the ReadObject
	 * @param InReadObject - The storage location for the returned leaderboard data
	 */
	FOnlineAsyncTaskLiveGetLeaderboardForUsers(
			FOnlineSubsystemLive* InLiveSubsystem,
			TArray<FOnlineLeaderboardReadRef> InLeaderboardReads,
			const FOnlineLeaderboardReadRef& InReadObject);

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const {
		const bool WasSuccessful = bWasSuccessful;
		return FString::Printf(TEXT("FOnlineAsyncTaskLiveGetLeaderboardForUsers bWasSuccessful: %d"), WasSuccessful);}
	virtual void Tick();
	virtual void Finalize();
	virtual void TriggerDelegates();

private:
	/** List of ReadObjects to be populated by single user leaderboard requests */
	TArray<FOnlineLeaderboardReadRef> LeaderboardReads;
	/** Handle to the read object where the data will be stored */
	FOnlineLeaderboardReadPtr ReadObject;
};
