// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlinePresenceInterfaceLive.h"

using namespace Microsoft::Xbox::Services::Presence;
using namespace Microsoft::Xbox::Services::UserStatistics;

/**
 * Custom task class that handles getting presence info as well as configured stats in parallel.
 */
class FOnlineAsyncTaskLiveQueryPresence : public FOnlineAsyncTaskBasic<FOnlineSubsystemLive>
{
public:
	FOnlineAsyncTaskLiveQueryPresence(
			FOnlineSubsystemLive* const InLiveSubsystem,
			Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
			const FUniqueNetIdLive& InUser,
			const IOnlinePresence::FOnPresenceTaskCompleteDelegate& InDelegate)
		: FOnlineAsyncTaskBasic(InLiveSubsystem)
		, LiveContext(InLiveContext)
		, User(InUser)
		, Delegate(InDelegate)
	{
	}

	virtual FString ToString() const override
	{
		return FString(TEXT("FOnlineAsyncTaskLiveQueryPresence"));
	}

	virtual void Initialize() override;
	virtual void Tick() override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	/** The Xbox Live context of the user requesting presence information. */
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext;

	/** The user whose presence data is being queried. */
	FUniqueNetIdLive User;

	/** Stored delegate passed in from IOnlinePesence::QueryPresence, to be triggered in TriggerDelegates(). */
	IOnlinePresence::FOnPresenceTaskCompleteDelegate Delegate;

	/** Stored task to retrieve the user's PresenceRecord, always runs. */
	Concurrency::task<Microsoft::Xbox::Services::Presence::PresenceRecord^> PresenceTask;

	/**
	 * The stats task is optional in case no stats for presence have been configured.
	 * There doesn't appear to be a way to determine if a valid task has been assigned to a default-constructed task object.
	 */
	TOptional<Concurrency::task<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult^>> StatsTask;

	/** Result of the async presence query. Used to update cached data in Finalize(). */
	Microsoft::Xbox::Services::Presence::PresenceRecord^ PresenceResult;

	/** Result of the async stats query. Used to update cached data in Finalize(). */
	Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult^ StatsResult;
};
