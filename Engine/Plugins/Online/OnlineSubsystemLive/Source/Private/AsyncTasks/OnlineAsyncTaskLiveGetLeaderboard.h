// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemLive.h"
#include "OnlineStats.h"

using namespace Microsoft::Xbox::Services::Leaderboard;

/**
 *	Async task to retrieve a single user's leaderboard from Live
 */
class FOnlineAsyncTaskLiveGetLeaderboard : public FOnlineAsyncItem
{
public:
	/**
	 * Constructor
	 *
	 * @param InLiveSubsystem - The LiveOnlineSubsystem used to retrieve the leaderboards interface
	 * @param InResults - The leaderboard data returned from Live
	 * @param InReadObject - The storage location for the returned leaderboard data
	 * @param InFireDelegates - Whether or not to fire the OnLeaderboardReadComplete delegate
	 */
	FOnlineAsyncTaskLiveGetLeaderboard(
			FOnlineSubsystemLive* InLiveSubsystem,
			LeaderboardResult^ InResults,
			const FOnlineLeaderboardReadRef& InReadObject,
			bool InFireDelegates);

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveGetLeaderboard");}
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	/** Reference to the Live online subsystem */
	FOnlineSubsystemLive* LiveSubsystem;
	
	/** Pointer to the leaderboard returned by the async request */
	LeaderboardResult^ Results;

	/** Handle to the read object where the data will be stored */
	FOnlineLeaderboardReadPtr ReadObject;

	/** Whether or not to fire the OnLeaderboardReadCompleteDelegates on completion */
	bool bFireDelegates;

	/**
	* Converts the LeaderboardResult string data to the type requested in the LeaderboardRead metadata
	*
	* @param RowData - The string containing the value to be parsed
	* @param FromType - The value type of the column in the leaderboard result. This is compared to the ToType to verify the conversion is appropriate.
	* @param ToType - The type of data to parse from the string and return
	* @return An FVariantData containing the requested datatype parsed from the RowData string
	*/
	FVariantData ConvertLeaderboardRowDataToRequestedType(const TCHAR* RowData, Windows::Foundation::PropertyType FromType, EOnlineKeyValuePairDataType::Type ToType);

	/**
	* Reports a warning that the FromType and the ToType are mismatched and the conversion from one to the other may not be accurate
	*
	* @param FromType - The Windows PropertyType that is being converted from
	* @param ToType - The EOnlineKeyValuePairDataType that is being converted to
	*/
	void ReportTypeMismatchWarning(Windows::Foundation::PropertyType FromType, EOnlineKeyValuePairDataType::Type ToType);
};

/**
 *	Async task to notify delegates of a leaderboard read failure on the game thread.
 */
class FOnlineAsyncTaskLiveGetLeaderboardFailed : public FOnlineAsyncItem
{
public:
	/**
	 * Constructor
	 *
	 * @param InLiveSubsystem - The LiveOnlineSubsystem used to retrieve the leaderboards interface
	 */
	FOnlineAsyncTaskLiveGetLeaderboardFailed(FOnlineSubsystemLive* InLiveSubsystem);

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveGetLeaderboardFailed");}
	virtual void TriggerDelegates() override;

private:
	/** Reference to the Live online subsystem */
	FOnlineSubsystemLive* LiveSubsystem;
};
