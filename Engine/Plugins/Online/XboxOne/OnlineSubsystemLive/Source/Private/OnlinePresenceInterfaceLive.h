// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlinePresenceInterface.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemLivePackage.h"

/** 
 * Implementation for the Live rich presence interface
 */
class FOnlinePresenceLive : public IOnlinePresence
{
PACKAGE_SCOPE:
	/** Constructor
	 *
	 * @param InSubsystem The owner of this external UI interface.
	 */
	explicit FOnlinePresenceLive(class FOnlineSubsystemLive* InSubsystem) :
		LiveSubsystem(InSubsystem)
	{
	}

	/** Reference to the owning subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

	// @ATG_CHANGE : BEGIN Adding social features
	TSharedRef<FOnlineUserPresence> CachePresenceFromLive(Microsoft::Xbox::Services::Presence::PresenceRecord^ Record);
	TSharedRef<FOnlineUserPresence> CachePresenceFromLive(const FUniqueNetIdLive& UserLive, Microsoft::Xbox::Services::Social::Manager::SocialManagerPresenceRecord^ Record);
	// @ATG_CHANGE : END

public:
	// IOnlinePresence
	virtual void SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual void QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual EOnlineCachedResult::Type GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence) override;
	virtual EOnlineCachedResult::Type GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence) override;

private:
	/**
	 *	Async event that notifies when a set presence operation has completed.
	 */
	class FAsyncEventSetPresenceCompleted : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventSetPresenceCompleted() :
			FOnlineAsyncEvent(NULL),
			User(nullptr)
		{
		}

		/** The user whose presence was set. */
		FUniqueNetIdLive User;

		/** True if the set presence operation succeeded, false if it didn't. */
		bool bWasSuccessful;

		/** Delegate to execute on the game thread to notify it that the operation is complete. */
		FOnPresenceTaskCompleteDelegate Delegate;

	public:
		/**
		 * Constructor.
		 *
		 * @param InLiveSubsystem The owner of the external UI interface that triggered this event.
		 * @param InUser The user whose presence was set.
		 * @param InWasSuccessful True if the set presence operation succeeded, false if it didn't.
		 */
		FAsyncEventSetPresenceCompleted(FOnlineSubsystemLive* InLiveSubsystem, const FUniqueNetIdLive& InUser, const bool InWasSuccessful, const FOnPresenceTaskCompleteDelegate& InDelegate) :
			FOnlineAsyncEvent(InLiveSubsystem),
			User(InUser),
			bWasSuccessful(InWasSuccessful),
			Delegate(InDelegate)
		{
		}

		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	/**
	 *	Async event that notifies when a Query presence operation has completed.
	 */
	class FAsyncEventQueryCompleted : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventQueryCompleted() :
			FOnlineAsyncEvent(nullptr)
		{
		}

		/** The collection of user ids requested */
		FUniqueNetIdLive User;

		/** The PresenceRecord retrieved for the user. */
		Microsoft::Xbox::Services::Presence::PresenceRecord^ Record;
	
		/** True if the set presence operation succeeded, false if it didn't. */
		bool bWasSuccessful;

		/** Delegate to execute on the game thread to notify it that the operation is complete. */
		FOnPresenceTaskCompleteDelegate Delegate;

	public:
		/**
		 * Constructor.
		 *
		 * @param InLiveSubsystem The owner of the external UI interface that triggered this event.
		 * @param InUsers The users whose presence was retrieved.
		 * @param InRecord The presence information for the user.
		 * @param InWasSuccessful True if the set presence operation succeeded, false if it didn't.
		 */
		FAsyncEventQueryCompleted(FOnlineSubsystemLive* InLiveSubsystem,
								  const FUniqueNetId& InUser,
								  Microsoft::Xbox::Services::Presence::PresenceRecord^ InRecord,
								  const bool InWasSuccessful,
								  const FOnPresenceTaskCompleteDelegate& InDelegate) :
			FOnlineAsyncEvent(InLiveSubsystem),
			User(InUser),
			Record(InRecord),
			bWasSuccessful(InWasSuccessful),
			Delegate(InDelegate)
		{
		}

		virtual void Finalize() override;
		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	// @ATG_CHANGE : BEGIN Adding social features
	/** Cache of presence data. Stores only the most recent results of QueryPresence. */
	TMap<FUniqueNetIdLive, TSharedRef<FOnlineUserPresence>> PresenceCache;
	// @ATG_CHANGE : END
};

typedef TSharedPtr<FOnlinePresenceLive, ESPMode::ThreadSafe> FOnlinePresenceLivePtr;

