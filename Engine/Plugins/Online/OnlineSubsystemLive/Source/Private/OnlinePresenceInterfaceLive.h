// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlinePresenceInterface.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemLivePackage.h"

/**
 * Live Presence record to handle constructing from a XBox Live PresenceRecord
 */
class FOnlineUserPresenceLive
	: public FOnlineUserPresence
{
PACKAGE_SCOPE:
	/**
	 * Default constructor
	 */
	FOnlineUserPresenceLive() = default;
	virtual ~FOnlineUserPresenceLive() = default;
	FOnlineUserPresenceLive(const FOnlineUserPresenceLive& Other) = default;
	FOnlineUserPresenceLive(FOnlineUserPresenceLive&& Other) = default;
	FOnlineUserPresenceLive& operator=(const FOnlineUserPresenceLive& Other) = default;
	FOnlineUserPresenceLive& operator=(FOnlineUserPresenceLive&& Other) = default;

	/**
	 * Construct our values from an XBox presence record
	 */
	explicit FOnlineUserPresenceLive(Microsoft::Xbox::Services::Presence::PresenceRecord^ Record)
	{
		SetPresenceFromPresenceRecord(Record);
	}

	void SetPresenceFromPresenceRecord(Microsoft::Xbox::Services::Presence::PresenceRecord^ Record)
	{
		switch (Record->UserState)
		{
		case Microsoft::Xbox::Services::Presence::UserPresenceState::Online:
			Status.State = EOnlinePresenceState::Online;
			bIsOnline = true;
			// @ATG_CHANGE : Improved Social support - BEGIN
			bIsPlaying = true;
			// @ATG_CHANGE : END
			break;
		case Microsoft::Xbox::Services::Presence::UserPresenceState::Away:
			Status.State = EOnlinePresenceState::Away;
			bIsOnline = true;
			// @ATG_CHANGE :Improved Social support - BEGIN
			bIsPlaying = true;
			// @ATG_CHANGE : END
			break;
		case Microsoft::Xbox::Services::Presence::UserPresenceState::Offline:
		case Microsoft::Xbox::Services::Presence::UserPresenceState::Unknown:
		default:
			Status.State = EOnlinePresenceState::Offline;
			// @ATG_CHANGE :Improved Social support - BEGIN
			bIsOnline = false;
			bIsPlaying = false;
			// @ATG_CHANGE : UWP Live Support - END
			break;
		}

		// We have to enumerate through all of the users devices, and then all of the user's titles on those devices to find our own title
		// @ATG_CHANGE : Improved Social support - BEGIN
		bIsPlayingThisGame = false;
		Status.Properties.Empty();
		if (Record->PresenceDeviceRecords->Size > 0)
		{
			Microsoft::Xbox::Services::XboxLiveAppConfiguration^ AppConfig = Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance;
			check(AppConfig != nullptr);

			Microsoft::Xbox::Services::Presence::PresenceDeviceRecord ^PopulateBasedOnDeviceRecord = Record->PresenceDeviceRecords->GetAt(0);
			Microsoft::Xbox::Services::Presence::PresenceTitleRecord ^PopulateBasedOnTitleRecord = nullptr;
			for (auto DeviceRecord : Record->PresenceDeviceRecords)
			{
				for (auto TitleRecord : DeviceRecord->PresenceTitleRecords)
				{
					if (PopulateBasedOnTitleRecord == nullptr)
					{
						PopulateBasedOnTitleRecord = TitleRecord;
					}
					else if (AppConfig->TitleId == TitleRecord->TitleId)
					{
						PopulateBasedOnTitleRecord = TitleRecord;
						PopulateBasedOnDeviceRecord = DeviceRecord;
						break;
					}
				}
			}

			Status.Properties.Add(DefaultPlatformKey, PopulateBasedOnDeviceRecord->DeviceType.ToString()->Data());

			if (PopulateBasedOnTitleRecord != nullptr)
			{
				bIsPlayingThisGame = PopulateBasedOnTitleRecord->TitleId == AppConfig->TitleId;

				Status.Properties.Add(DefaultAppIdKey, PopulateBasedOnTitleRecord->TitleName->Data());

				// Use the Live Presence string as UE's StatusStr (consistent with other online platforms)
				Status.StatusStr = PopulateBasedOnTitleRecord->Presence->Data();

				// Also set the same string under the DefaultPresenceKey (consistent with original OSSLive behavior)
				Status.Properties.Add(DefaultPresenceKey, Status.StatusStr);
			}
		}
		// @ATG_CHANGE : END
	}

	// @ATG_CHANGE : Improved Social support - BEGIN
	explicit FOnlineUserPresenceLive(Microsoft::Xbox::Services::Social::Manager::SocialManagerPresenceRecord^ Record);
	// @ATG_CHANGE : END

	/**
	 * Add status key/value properties based on Xbox statistics.
	 */
	void SetStatusPropertiesFromStatistics(Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult^ StatsResult);
};

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
	explicit FOnlinePresenceLive(class FOnlineSubsystemLive* InSubsystem)
		: LiveSubsystem(InSubsystem)
	{
		check(LiveSubsystem);
	}

	// @ATG_CHANGE : BEGIN Adding social features
	TSharedRef<FOnlineUserPresenceLive> CachePresenceFromLive(Microsoft::Xbox::Services::Presence::PresenceRecord^ Record);
	TSharedRef<FOnlineUserPresenceLive> CachePresenceFromLive(const FUniqueNetIdLive& UserLive, Microsoft::Xbox::Services::Social::Manager::SocialManagerPresenceRecord^ Record);
	// @ATG_CHANGE : END

public:
	// IOnlinePresence
	virtual void SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual void QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual EOnlineCachedResult::Type GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence) override;
	virtual EOnlineCachedResult::Type GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence) override;

	/**
	 * Returns an IVectorView of statistic names that have been set by the PresenceStats array in the [OnlineSubsystemLive] config section.
	 * This view is suitable for passing to GetSingleUserStatisticsAsync, GetMultipleUserStatisticsAsync, or related functions.
	 * These stats should be added as properties in the FOnlineUserPresence results of presence queries.
	 */
	static Windows::Foundation::Collections::IVectorView<Platform::String^>^ GetConfiguredPresenceStatNames();

PACKAGE_SCOPE:
	void OnPresenceDeviceChanged(Microsoft::Xbox::Services::Presence::DevicePresenceChangeEventArgs^ Args);
	void OnPresenceTitleChanged(Microsoft::Xbox::Services::Presence::TitlePresenceChangeEventArgs^ Args);

	void OnSessionUpdatedStatChange(const FUniqueNetIdLive& LocalUserId, const FUniqueNetIdLive& UpdatedSessionPlayerId);

	bool IsSubscribedToPresenceUpdates(const FUniqueNetIdLive& LiveUserId) const;
	void AddPresenceUpdateSubscription(const FUniqueNetIdLive& LiveUserId);

	void OnFindFriendSessionCompleteForSessionUpdated(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& SessionSearchResult, const FUniqueNetIdLive& FriendId);

private:
	/**
	 *	Async event that notifies when a set presence operation has completed.
	 */
	class FAsyncEventSetPresenceCompleted : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventSetPresenceCompleted() = delete;

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

PACKAGE_SCOPE:
	/** Reference to the owning subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

private:
	friend class FOnlineAsyncTaskLiveQueryFriendManagerTask;
	friend class FOnlineAsyncTaskLiveQueryPresence;

	TArray<FDelegateHandle> FriendSessionDelegateHandles;

	/** Cache of local user's last saved presence to prevent duplicate stores */
	TMap<FUniqueNetIdLive, FOnlineUserPresenceStatus> LocalUserPresenceCache;

	/** Cache of presence data. Stores only the most recent results of QueryPresence. */
	TMap<FUniqueNetIdLive, TSharedRef<FOnlineUserPresenceLive>> PresenceCache;

	/** List of users we're subscribed to for presence updates */
	TSet<FUniqueNetIdLive> PresenceSubscriptionSet;

	/** Helper function to get the presence string from presence status */
	static FString GetPresenceIdString(const FOnlineUserPresenceStatus& Status);
};

typedef TSharedPtr<FOnlinePresenceLive, ESPMode::ThreadSafe> FOnlinePresenceLivePtr;

