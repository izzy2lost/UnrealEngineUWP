// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlinePresenceInterfaceLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineFriendsInterfaceLive.h"
#include "OnlineSessionInterfaceLive.h"
#include "OnlineSubsystemLive.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch
#include "OnlineEventsInterface.h"

using namespace Microsoft::Xbox::Services::Presence;
// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
using namespace Microsoft::Xbox::Services::Social::Manager;
// @ATG_CHANGE : END
using namespace Platform;
using Microsoft::Xbox::Services::Presence::PresenceDeviceType;

void FOnlinePresenceLive::SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	const FUniqueNetIdLive UserLive(User);

	const FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	if (!Identity.IsValid())
	{
		LiveSubsystem->ExecuteNextTick([Delegate, UserLive]()
		{
			UE_LOG_ONLINE(Warning, TEXT("SetPresence failed. Bad Identity interface."));
			Delegate.ExecuteIfBound(UserLive, false);
		});
		return;
	}

	// Find the User^ associated with this unique net id.
	Windows::Xbox::System::User^ PresenceUser = Identity->GetUserForUniqueNetId(UserLive);
	if (!PresenceUser)
	{
		LiveSubsystem->ExecuteNextTick([Delegate, UserLive]()
		{
			UE_LOG_ONLINE(Warning, TEXT("SetPresence failed. No local user found."));
			Delegate.ExecuteIfBound(UserLive, false);
		});
		return;
	}

	FString GameStatusStr = Status.StatusStr;

	// Only support the default key for now, as a string.
	const FVariantData* PresenceId = Status.Properties.Find(DefaultPresenceKey);
	if (!PresenceId || PresenceId->GetType() != EOnlineKeyValuePairDataType::String)
	{
		PresenceId = nullptr;
	}

	// if we have no current status to set, return
	if (!PresenceId && GameStatusStr.IsEmpty())
	{
		LiveSubsystem->ExecuteNextTick([Delegate, UserLive]()
		{
			UE_LOG_ONLINE(Warning, TEXT("SetPresence failed. No presence value was set."));
			Delegate.ExecuteIfBound(UserLive, false);
		});
		return;
	}

	FString PresenceIdString;
	if (PresenceId)
	{
		PresenceId->GetValue(PresenceIdString);
	}
	else
	{
		PresenceIdString = GameStatusStr;
	}

	// Ensure we're not posting our current presence again
	{
		FString* CurrentUsersPresencePtr = LocalUserPresenceCache.Find(UserLive);
		if (CurrentUsersPresencePtr != nullptr)
		{
			if (CurrentUsersPresencePtr->Equals(PresenceIdString))
			{
				LiveSubsystem->ExecuteNextTick([Delegate, UserLive]()
				{
					constexpr const bool bWasSuccessful = true;
					Delegate.ExecuteIfBound(UserLive, bWasSuccessful);
				});
				return;
			}
		}
		LocalUserPresenceCache.Add(UserLive, PresenceIdString);
	}

	try
	{
		Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(PresenceUser);

		// before setting the presence queue up the stat events to trigger
		IOnlineEventsPtr EventsInterface = LiveSubsystem->GetEventsInterface();
		if (EventsInterface.IsValid())
		{
			for (const FPresenceProperties::ElementType& PropertyPair : Status.Properties)
			{
				static const FString EventPrefix(TEXT("Event_"));
				if (PropertyPair.Key.StartsWith(EventPrefix, ESearchCase::IgnoreCase))
				{
					FString DataString;
					PropertyPair.Value.GetValue(DataString);

					FOnlineEventParms Parms;
					Parms.Add(TEXT("Value"), DataString);

					EventsInterface->TriggerEvent(UserLive, *PropertyPair.Key, Parms);
				}
			}
		}

		// @ATG_CHANGE : BEGIN - UWP LIVE support
		PresenceData^ Data = ref new PresenceData(LiveContext->AppConfig->ServiceConfigurationId,
		// @ATG_CHANGE : END
												  ref new Platform::String(*PresenceIdString));

		Windows::Foundation::IAsyncAction^ SetPresenceAction = LiveContext->PresenceService->SetPresenceAsync(true, Data);

		concurrency::create_task(SetPresenceAction).then([this, UserLive, Status, Delegate](concurrency::task<void> Task)
		{
			bool bSuccess = true;

			try
			{
				Task.get();
				// Success if get() didn't throw.

				// Save our new presence into our cache
				LiveSubsystem->ExecuteNextTick([this, UserLive, Status]()
				{
					TSharedRef<FOnlineUserPresenceLive> Presence = MakeShared<FOnlineUserPresenceLive>();
					Presence->bIsOnline = true;
					Presence->bIsPlaying = true;
					Presence->bIsPlayingThisGame = true;
					Presence->Status = Status;
					PresenceCache.Add(UserLive, Presence);
				});

				UE_LOG_ONLINE(Display, TEXT("SetPresenceAsync succeeded."));
			}
			catch(Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("SetPresenceAsync failed at Task.get(). Exception: %s."), Ex->ToString()->Data());
				bSuccess = false;
			}

			// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
			LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventSetPresenceCompleted>(LiveSubsystem, UserLive, bSuccess, Delegate);
		});
	}
	catch(Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("SetPresenceAsync failed. Exception: %s."), Ex->ToString()->Data());
	}
}

void FOnlinePresenceLive::QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	const FUniqueNetIdLive UserLive(User);

	// This interface is wrong and doesn't specify the user to query from, or the users to query, either way.
	// For now we'll treat the User as the person to query, and later there'll be an array of users to query
	// and we'll query them FROM user instead.

	// Try to find a user to query presence from
	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = nullptr;
	{
		for (int32 ControllerIndex = 0; ControllerIndex < MAX_LOCAL_PLAYERS && LiveContext == nullptr; ++ControllerIndex)
		{
			LiveContext = LiveSubsystem->GetLiveContext(ControllerIndex);
		}

		if (!LiveContext)
		{
			LiveSubsystem->ExecuteNextTick([Delegate, UserLive]()
			{
				UE_LOG_ONLINE(Warning, TEXT("Could not query presence, could not find a live context to use"));
				Delegate.ExecuteIfBound(UserLive, false);
			});
			return;
		}
	}

	const FOnlinePresenceLivePtr PresencePtr = LiveSubsystem->GetPresenceLive();

	try
	{
		Platform::String^ UserToQueryXuid = ref new Platform::String(*UserLive.UniqueNetIdStr);

		// Subscribe to presence updates
		if (PresencePtr.IsValid())
		{
			if (!PresencePtr->IsSubscribedToPresenceUpdates(UserLive))
			{
				PresencePtr->AddPresenceUpdateSubscription(UserLive);
				try
				{
					// Subscribe to updates
					LiveContext->PresenceService->SubscribeToTitlePresenceChange(UserToQueryXuid, LiveSubsystem->TitleId);
					LiveContext->PresenceService->SubscribeToDevicePresenceChange(UserToQueryXuid);
				}
				catch (Platform::Exception^ Ex)
				{
					UE_LOG_ONLINE(Warning, TEXT("Failed to subscribe to presence updates for user %s"), *UserLive.ToString());
				}
			}
		}

		// Register for session updates via stats
		FOnlineSessionLivePtr SessionInt(LiveSubsystem->GetSessionInterfaceLive());
		if (SessionInt.IsValid())
		{
			if (!SessionInt->IsSubscribedToSessionStatUpdates(UserLive))
			{
				SessionInt->AddSessionUpdateStatSubscription(UserLive);
				Platform::String^ SessionUpdatedStatName = ref new Platform::String(*SessionInt->SessionUpdateStatName);
				// @ATG_CHANGE : BEGIN - UWP support
				LiveContext->UserStatisticsService->SubscribeToStatisticChange(UserToQueryXuid, ::Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->ServiceConfigurationId, SessionUpdatedStatName);
				// @ATG_CHANGE : END - UWP support
			}
		}

		auto GetPresenceOp = LiveContext->PresenceService->GetPresenceAsync(UserToQueryXuid);
		concurrency::create_task(GetPresenceOp).then([this, UserLive, Delegate](concurrency::task<PresenceRecord^> Task)
		{
			PresenceRecord^ Results = nullptr;
			bool bWasSuccessful = false;

			try
			{
				Results = Task.get();
				bWasSuccessful = true;
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("The get presence task failed. Exception: %ls."), Ex->ToString()->Data());
			}

			// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
			LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventQueryCompleted>(LiveSubsystem, UserLive, Results, bWasSuccessful, Delegate);
		});
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("Getting presence failed. Exception: %s."), Ex->ToString()->Data());
	}
}

EOnlineCachedResult::Type FOnlinePresenceLive::GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	TSharedRef<FOnlineUserPresenceLive>* Presence = PresenceCache.Find(static_cast<const FUniqueNetIdLive&>(User));
	if (!Presence)
	{
		return EOnlineCachedResult::NotFound;
	}

	OutPresence = *Presence;
	return EOnlineCachedResult::Success;
}

EOnlineCachedResult::Type FOnlinePresenceLive::GetCachedPresenceForApp(const FUniqueNetId& /*LocalUserId*/, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	EOnlineCachedResult::Type Result = EOnlineCachedResult::NotFound;

	if (LiveSubsystem->GetAppId() == AppId)
	{
		Result = GetCachedPresence(User, OutPresence);
	}

	return Result;
}

void FOnlinePresenceLive::OnPresenceDeviceChanged(DevicePresenceChangeEventArgs^ Args)
{
	const FUniqueNetIdLive UserLive(Args->XboxUserId);

	TSharedRef<FOnlineUserPresenceLive>* UserPresencePtr = PresenceCache.Find(UserLive);
	if (!UserPresencePtr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Received presence update for unknown player %s"), *UserLive.ToString());
		return;
	}
	// Update Presence values
	TSharedRef<FOnlineUserPresenceLive>& Presence = *UserPresencePtr;
	Presence->bIsOnline = Args->IsUserLoggedOnDevice;

	// Attempt to update presence for anyone we have on our friends list
	FOnlineFriendsLivePtr FriendsInt = LiveSubsystem->GetFriendsLive();
	if (FriendsInt.IsValid())
	{
		FriendsInt->OnUserPresenceUpdate(UserLive, Presence);
	}

	// Trigger presence delegate
	TriggerOnPresenceReceivedDelegates(UserLive, Presence);
}

void FOnlinePresenceLive::OnPresenceTitleChanged(TitlePresenceChangeEventArgs^ Args)
{
	const FUniqueNetIdLive UserLive(Args->XboxUserId);

	TSharedRef<FOnlineUserPresenceLive>* UserPresencePtr = PresenceCache.Find(UserLive);
	if (!UserPresencePtr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Received presence update for unknown player %s"), *UserLive.ToString());
		return;
	}

	// Update Presence values
	TSharedRef<FOnlineUserPresenceLive>& Presence = *UserPresencePtr;
	Presence->bIsPlaying = Args->TitleState == TitlePresenceState::Started;
	Presence->bIsPlayingThisGame = Presence->bIsPlaying && Args->TitleId == LiveSubsystem->TitleId;

	// Attempt to update presence for anyone we have on our friends list
	FOnlineFriendsLivePtr FriendsInt = LiveSubsystem->GetFriendsLive();
	if (FriendsInt.IsValid())
	{
		FriendsInt->OnUserPresenceUpdate(UserLive, Presence);
	}

	// Trigger presence delegate
	TriggerOnPresenceReceivedDelegates(UserLive, Presence);
}

void FOnlinePresenceLive::OnSessionUpdatedStatChange(const FUniqueNetIdLive& LocalUserId, const FUniqueNetIdLive& UpdatedSessionPlayerId)
{
	FOnlineSessionLivePtr SessionInt(LiveSubsystem->GetSessionInterfaceLive());
	if (!SessionInt.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Unable to update presence session, session interface is bad"));
		return;
	}

	FOnlineIdentityLivePtr IdentityInt(LiveSubsystem->GetIdentityLive());
	if (!IdentityInt.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Unable to update presence session, identity interface is bad"));
		return;
	}

	int32 LocalUserNum = IdentityInt->GetControllerIndexForId(LocalUserId);
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		UE_LOG_ONLINE(Warning, TEXT("Unable to update presence session, bad localplayernum"));
		return;
	}

	SessionInt->FindFriendSession(LocalUserId, UpdatedSessionPlayerId);
}

void FOnlinePresenceLive::OnFindFriendSessionCompleteForSessionUpdated(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& SessionSearchResult, const FUniqueNetIdLive& FriendId)
{
	UE_LOG_ONLINE(Verbose, TEXT("OnFindFriendSessionCompleteForSessionUpdated LocalUserNum: %d bWasSuccessful: %d SessionValid: %d"), LocalUserNum, bWasSuccessful, SessionSearchResult.IsValid());

	FOnlineIdentityLivePtr IdentityPtr = LiveSubsystem->GetIdentityLive();
	if (!IdentityPtr.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("OnFindFriendSessionCompleteForSessionUpdated Could not find identity interface"));
		return;
	}

	TSharedPtr<const FUniqueNetIdLive> LocalNetId(StaticCastSharedPtr<const FUniqueNetIdLive>(IdentityPtr->GetUniquePlayerId(LocalUserNum)));
	if (!LocalNetId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("OnFindFriendSessionCompleteForSessionUpdated Could not find LocalUserNum: %d"), LocalUserNum);
		return;
	}

	TSharedPtr<const FUniqueNetId> NewSessionId;
	if (bWasSuccessful)
	{
		// Only set session id if we were successful
		NewSessionId = MakeShared<const FUniqueNetIdString>(SessionSearchResult.GetSessionIdStr());
	}

	const bool bHasEmptySlot = SessionSearchResult.Session.NumOpenPublicConnections > 0;
	const bool bSessionOpen = SessionSearchResult.Session.SessionSettings.bAllowJoinInProgress;

	const bool bIsJoinable = bWasSuccessful && bHasEmptySlot && bSessionOpen;

	// Update Presence values
	TSharedRef<FOnlineUserPresenceLive>* UserPresencePtr = PresenceCache.Find(FriendId);
	if (UserPresencePtr)
	{
		TSharedRef<FOnlineUserPresenceLive>& Presence = *UserPresencePtr;

		Presence->bIsJoinable = bIsJoinable;
		Presence->SessionId = NewSessionId;

		TriggerOnPresenceReceivedDelegates(*LocalNetId, Presence);
	}

	// Attempt to update presence for anyone we have on our friends list
	FOnlineFriendsLivePtr FriendsInt = LiveSubsystem->GetFriendsLive();
	if (FriendsInt.IsValid())
	{
		FriendsInt->OnUserSessionPresenceUpdate(FriendId, NewSessionId, bIsJoinable);
	}
}

bool FOnlinePresenceLive::IsSubscribedToPresenceUpdates(const FUniqueNetIdLive& LiveUserId) const
{
	return PresenceSubscriptionSet.Contains(LiveUserId);
}

void FOnlinePresenceLive::AddPresenceUpdateSubscription(const FUniqueNetIdLive& LiveUserId)
{
	PresenceSubscriptionSet.Add(LiveUserId);
}

FString FOnlinePresenceLive::FAsyncEventSetPresenceCompleted::ToString() const
{
	return TEXT("Set presence complete.");
}

void FOnlinePresenceLive::FAsyncEventSetPresenceCompleted::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	Delegate.ExecuteIfBound(User, bWasSuccessful);
}

FString FOnlinePresenceLive::FAsyncEventQueryCompleted::ToString() const
{
	return TEXT("Query presence complete.");
}

void FOnlinePresenceLive::FAsyncEventQueryCompleted::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	Delegate.ExecuteIfBound(User, bWasSuccessful);

	if (bWasSuccessful)
	{
		TSharedPtr<FOnlineUserPresence> Presence;
		if (Subsystem &&
			Subsystem->GetPresenceLive().IsValid() &&
			Subsystem->GetPresenceLive()->GetCachedPresence(User, Presence) == EOnlineCachedResult::Success &&
			Presence.IsValid())
		{
			Subsystem->GetPresenceLive()->TriggerOnPresenceReceivedDelegates(User, Presence.ToSharedRef());
		}
	}
}

void FOnlinePresenceLive::FAsyncEventQueryCompleted::Finalize()
{
	FOnlineAsyncEvent::Finalize();

	if(!Subsystem || !Subsystem->GetPresenceLive().IsValid())
	{
		bWasSuccessful = false;
		return;
	}

// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
	if(!Record || !Record->PresenceDeviceRecords)
	{
		bWasSuccessful = false;
		return;
	}

	Subsystem->GetPresenceLive()->CachePresenceFromLive(Record);
	bWasSuccessful = true;
	// @ATG_CHANGE : END
}

// @ATG_CHANGE : BEGIN - Alternative Social implementation using Manager 
TSharedRef<FOnlineUserPresenceLive> FOnlinePresenceLive::CachePresenceFromLive(PresenceRecord ^Record)
{
	const FUniqueNetIdLive UserLive(Record->XboxUserId->Data());
	auto ExistingPresence = PresenceCache.Find(UserLive);
	auto& PresenceToUpdate = ExistingPresence ? *ExistingPresence : PresenceCache.Add(UserLive, MakeShareable(new FOnlineUserPresenceLive(Record)));

	return PresenceToUpdate;
}

TSharedRef<FOnlineUserPresenceLive> FOnlinePresenceLive::CachePresenceFromLive(const FUniqueNetIdLive& UserLive, SocialManagerPresenceRecord ^Record)
{
	auto ExistingPresence = PresenceCache.Find(UserLive);
	auto& PresenceToUpdate = ExistingPresence ? *ExistingPresence : PresenceCache.Add(UserLive, MakeShareable(new FOnlineUserPresenceLive(Record)));
	return PresenceToUpdate;
}

FOnlineUserPresenceLive::FOnlineUserPresenceLive(SocialManagerPresenceRecord ^Record)
{
	switch (Record->UserState)
	{
	case UserPresenceState::Online:
		bIsOnline = true;
		bIsPlaying = true;
		Status.State = EOnlinePresenceState::Online;
		break;

	case UserPresenceState::Away:
		bIsOnline = true;
		bIsPlaying = true;
		Status.State = EOnlinePresenceState::Away;
		break;

	case UserPresenceState::Offline:
		bIsOnline = false;
		bIsPlaying = false;
		Status.State = EOnlinePresenceState::Offline;
		break;

	case UserPresenceState::Unknown:
	default:
		break;
	}

	Microsoft::Xbox::Services::XboxLiveAppConfiguration^ AppConfig = Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance;
	check(AppConfig != nullptr);

	SocialManagerPresenceTitleRecord ^PopulateBasedOnTitleRecord = nullptr;
	for (auto TitleRecord : Record->PresenceTitleRecords)
	{
		if (PopulateBasedOnTitleRecord == nullptr)
		{
			PopulateBasedOnTitleRecord = TitleRecord;
		}
		else if (AppConfig->TitleId == TitleRecord->TitleId)
		{
			PopulateBasedOnTitleRecord = TitleRecord;
			break;
		}
	}

	bIsPlayingThisGame = false;
	Status.Properties.Empty();
	if (PopulateBasedOnTitleRecord != nullptr)
	{
		Status.Properties.Add(DefaultPlatformKey, PopulateBasedOnTitleRecord->DeviceType.ToString()->Data());

		bIsPlayingThisGame = PopulateBasedOnTitleRecord->TitleId == AppConfig->TitleId;

		// Note: localized game name not available via social manager version of presence record

		// Use the Live Presence string as UE's StatusStr (consistent with other online platforms)
		Status.StatusStr = PopulateBasedOnTitleRecord->PresenceText->Data();

		// Also set the same string under the DefaultPresenceKey (consistent with original OSSLive behavior)
		Status.Properties.Add(DefaultPresenceKey, Status.StatusStr);
	}
}
// @ATG_CHANGE : END
