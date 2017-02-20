// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineLeaderboardInterface.h"

/**
 * Interface definition for the online services leaderboard services 
 */
class FOnlineLeaderboardsLive : public IOnlineLeaderboards
{
private:
	/** Reference to the main Live subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;
	/** when requesting a friends leaderboard this is the requested sort order for the leaderboard */
	static const TCHAR* SortOrder;

public:

	virtual ~FOnlineLeaderboardsLive() {}

	// IOnlineLeaderboards
	virtual bool ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual void FreeStats(FOnlineLeaderboardRead& ReadObject) override;
	virtual bool WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject) override;
	virtual bool FlushLeaderboards(const FName& SessionName) override;
	virtual bool WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores) override;

	/**
	 * Constructor
	 *
	 * @param InSubsystem - A reference to the owning subsystem
	 */
	explicit FOnlineLeaderboardsLive(FOnlineSubsystemLive* InLiveSubsystem);
};

typedef TSharedPtr<FOnlineLeaderboardsLive, ESPMode::ThreadSafe> FOnlineLeaderboardsLivePtr;
