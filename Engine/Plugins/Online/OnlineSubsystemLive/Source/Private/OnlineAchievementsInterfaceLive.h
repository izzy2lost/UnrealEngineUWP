// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlineAchievementsInterface.h"
#include "OnlineEventsInterfaceLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemLivePackage.h"
#include "OnlineJsonSerializer.h"

#include <collection.h>

using namespace Microsoft::Xbox::Services::Achievements;

class FAchievementsConfig : public FOnlineJsonSerializable
{
public:
	FString								AchievementEventName;
	FJsonSerializableKeyValueMapInt		AchievementMap;

	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE( "AchievementEventName", AchievementEventName );
	ONLINE_JSON_SERIALIZE_MAP( "AchievementMap", AchievementMap );
	END_ONLINE_JSON_SERIALIZER
};

/**
 *	FOnlineAchievementsLive - Interface class for achievements (Live implementation)
 */
class FOnlineAchievementsLive : public IOnlineAchievements
{
private:

	bool LoadAndInitFromJsonConfig( const TCHAR* JsonConfigName );
	void TestEventsAndAchievements();

	/** Pointer to owning live subsystem */
	class FOnlineSubsystemLive *							LiveSubsystem;

	/** Config settings initialized from json file */
	FAchievementsConfig										AchievementsConfig;

	/** Mapping of players to their achievements */
	TMap< FUniqueNetIdLive, TArray< FOnlineAchievement > > PlayerAchievements;

	/** Cached achievement descriptions for an Id */
	TMap< FString, FOnlineAchievementDesc >					AchievementDescriptions;

PACKAGE_SCOPE:

public:

	//~ Begin IOnlineAchievements Interface
	virtual void WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate = FOnAchievementsWrittenDelegate()) override;
	virtual void QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
	virtual void QueryAchievementDescriptions( const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate() ) override;
	virtual EOnlineCachedResult::Type GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement) override;
	virtual EOnlineCachedResult::Type GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement> & OutAchievements) override;
	virtual EOnlineCachedResult::Type GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc) override;
#if !UE_BUILD_SHIPPING
	virtual bool ResetAchievements( const FUniqueNetId& PlayerId ) override;
#endif // !UE_BUILD_SHIPPING
	//~ End IOnlineAchievements Interface

	/**
	 * Constructor
	 *
	 * @param InSubsystem - A reference to the owning subsystem
	 */
	FOnlineAchievementsLive(class FOnlineSubsystemLive* InSubsystem);

	
	/**
	 * Default destructor
	 */
	virtual ~FOnlineAchievementsLive();

private:
	/**
	 *	Async event that notifies when a Query achievements operation has completed.
	 */
	class FAsyncEventQueryCompleted : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventQueryCompleted() :
			FOnlineAsyncEvent( nullptr )
		{
		}

		/** The user who made the request */
		FUniqueNetIdLive						PlayerId;

		Platform::Collections::Vector<Achievement^>^	Achievements;

		/** True if the set presence operation succeeded, false if it didn't. */
		bool									bWasSuccessful;

		/** Delegate to execute on the game thread to notify it that the operation is complete. */
		FOnQueryAchievementsCompleteDelegate	Delegate;

	public:
		/**
		 * Constructor.
		 *
		 * @param InLiveSubsystem The owner of the external UI interface that triggered this event.
		 * @param InUsers The users whose presence was retrieved.
		 * @param InRecord The presence information for the user.
		 * @param InWasSuccessful True if the set presence operation succeeded, false if it didn't.
		 */
		FAsyncEventQueryCompleted(	FOnlineSubsystemLive *							InLiveSubsystem,
									const FUniqueNetIdLive &						InPlayerId,
									Platform::Collections::Vector<Achievement^>^	InAchievements,
									const bool										InWasSuccessful,
									const FOnQueryAchievementsCompleteDelegate &	InDelegate) :
			FOnlineAsyncEvent( InLiveSubsystem ),
			PlayerId( InPlayerId ),
			Achievements( InAchievements ),
			bWasSuccessful( InWasSuccessful ),
			Delegate( InDelegate )
		{
		}

		virtual void		Finalize() override;
		virtual FString		ToString() const override;
		virtual void		TriggerDelegates() override;
	};

	/** Process the AchievementsResult and query for the next "page" of results if necessary */
	void ProcessGetAchievementsResults(AchievementsResult^ Results, Platform::Collections::Vector<Achievement^>^ AllAchievements, const FUniqueNetIdLive UserLive, const FOnQueryAchievementsCompleteDelegate Delegate);
};

typedef TSharedPtr<FOnlineAchievementsLive, ESPMode::ThreadSafe> FOnlineAchievementsLivePtr;