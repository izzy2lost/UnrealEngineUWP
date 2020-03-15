// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlineLeaderboardInterface.h"

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

	// @ATG_CHANGE : BEGIN - Stats 2017 implementation using StatisticManager
#if USE_STATS_2017
	/** Reference to the StatisticManager singleton. Property access on a CX object is expensive so storing this will save a few cycles */
	Microsoft::Xbox::Services::Statistics::Manager::StatisticManager^ StatsManager;

	/** Stats 2017 tracks stat updates relative to the user and does not keep a concept of a session that would need to be flushed as StatisticManager
	will periodically flush in the background. This mapping allows the FlushLeaderboards function to work by mapping the session to the associated user.*/
	TArray<TTuple<FName, Windows::Xbox::System::User^>> SessionUserMapping;

	/** Users need to be added to the StatisticManager before stats are updated. Re-adding a user results in an exception. This list tracks
	which users have been added so we do not re-add a user.*/
	TArray<Windows::Xbox::System::User^> AddedUserList;
#endif
	// @ATG_CHANGE : END
public:

	// @ATG_CHANGE : BEGIN - Stats 2017 implementation using StatisticManager
	virtual ~FOnlineLeaderboardsLive();
	// @ATG_CHANGE : END

	// IOnlineLeaderboards
	virtual bool ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundUser(TSharedRef<const FUniqueNetId> Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
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

	// @ATG_CHANGE : BEGIN - Stats 2017 implementation using StatisticManager
	/** Stats tick (pumps statistic manager) */
	void Tick(float DeltaTime);
	// @ATG_CHANGE : END
};

typedef TSharedPtr<FOnlineLeaderboardsLive, ESPMode::ThreadSafe> FOnlineLeaderboardsLivePtr;
