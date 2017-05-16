// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlinePresenceInterfaceLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineAsyncTaskManagerLive.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch
#include "OnlineEventsInterface.h"

using namespace Microsoft::Xbox::Services::Presence;
using namespace Microsoft::Xbox::Services::Social::Manager;
using namespace Platform;

void FOnlinePresenceLive::SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	if(!LiveSubsystem)
	{
		return;
	}

	const auto Identity = LiveSubsystem->GetIdentityLive();
	if(!Identity.IsValid())
	{
		return;
	}

	// Find the User^ associated with this unique net id.
	const FUniqueNetIdLive UserLive(User);
	Windows::Xbox::System::User^ PresenceUser = Identity->GetUserForUniqueNetId(UserLive);
	if(!PresenceUser)
	{
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
		UE_LOG_ONLINE(Warning, TEXT("SetPresence failed. No presence value was set."));
		return;
	}
	
	try
	{
		Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(PresenceUser);
		
		// before setting the presence queue up the stat events to trigger
		IOnlineEventsPtr EventsInterface = LiveSubsystem->GetEventsInterface();
		TArray<FString> PropertyKeys;
		int NumPropertyKeys = Status.Properties.GetKeys(PropertyKeys);

		for (int i = 0; i < NumPropertyKeys; ++i)
		{
			FString Key = PropertyKeys[i];

			if (Key.StartsWith("Event_"))
			{
				const FVariantData* StatData = Status.Properties.Find(Key);
				FString DataString;
				StatData->GetValue(DataString);

				FOnlineEventParms Parms;
				Parms.Add(TEXT("Value"), DataString);

				EventsInterface->TriggerEvent(UserLive, Key.GetCharArray().GetData(), Parms);
			}
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

		// @ATG_CHANGE : jamesya@microsoft.com - BEGIN UWP LIVE support
		PresenceData^ Data = ref new PresenceData(LiveContext->AppConfig->ServiceConfigurationId,
												  ref new Platform::String(PresenceIdString.GetCharArray().GetData()));
		// @ATG_CHANGE : END
	
		Windows::Foundation::IAsyncAction^ SetPresenceAction = LiveContext->PresenceService->SetPresenceAsync(true, Data);

		concurrency::create_task(SetPresenceAction).then([=,this](concurrency::task<void> Task)
		{
			bool bSuccess = true;

			try
			{
				Task.get();
				// Success if get() didn't throw.

				UE_LOG_ONLINE(Display, TEXT("SetPresenceAsync succeeded."));
			}
			catch(Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("SetPresenceAsync failed at Task.get(). Exception: %s."), Ex->ToString()->Data());
				bSuccess = false;
			}

			// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
			if(LiveSubsystem->GetAsyncTaskManager())
			{
				auto NewEvent = new FAsyncEventSetPresenceCompleted(LiveSubsystem, UserLive, bSuccess, Delegate);
				LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
			}
		});
	}
	catch(Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("SetPresenceAsync failed. Exception: %s."), Ex->ToString()->Data());
	}
}

void FOnlinePresenceLive::QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	if(!LiveSubsystem)
	{
		return;
	}

	const auto Identity = LiveSubsystem->GetIdentityLive();
	if(!Identity.IsValid())
	{
		return;
	}

	const FUniqueNetIdLive UserLive(User);
	Windows::Xbox::System::User^ PresenceUser = Identity->GetUserForUniqueNetId(UserLive);
	if(!PresenceUser)
	{
		return;
	}

	try
	{
		Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(PresenceUser);
		
		auto GetPresence = LiveContext->PresenceService->GetPresenceAsync(PresenceUser->XboxUserId);

		concurrency::create_task(GetPresence).then([=,this](concurrency::task<PresenceRecord^> Task)
		{
			PresenceRecord^ Results = nullptr;
			bool bWasSuccessful = false;

			try
			{
				Results = Task.get();
				bWasSuccessful = true;
			}
			catch(Platform::COMException^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("The get presence task failed. Exception: %s."), Ex->ToString()->Data());
			}

			// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
			if(LiveSubsystem->GetAsyncTaskManager())
			{
				auto NewEvent = new FAsyncEventQueryCompleted(LiveSubsystem, UserLive, Results, bWasSuccessful, Delegate);
				LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
			}
		});
	}
	catch(Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("Getting presence failed. Exception: %s."), Ex->ToString()->Data());
	}
}

EOnlineCachedResult::Type FOnlinePresenceLive::GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	const FUniqueNetIdLive UserLive(User);
	if(!PresenceCache.Contains(UserLive))
	{
		return EOnlineCachedResult::NotFound;
	}

	OutPresence = PresenceCache.FindRef(UserLive);
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

	// @ATG_CHANGE : BEGIN Adding social features
	if (!Record)
	{
		bWasSuccessful = false;
		return;
	}

	Subsystem->GetPresenceLive()->CachePresenceFromLive(Record);
	bWasSuccessful = true;
	// @ATG_CHANGE : END
}

// @ATG_CHANGE : BEGIN Adding social features
TSharedRef<FOnlineUserPresence> FOnlinePresenceLive::CachePresenceFromLive(PresenceRecord ^Record)
{
	const FUniqueNetIdLive UserLive(Record->XboxUserId->Data());
	auto ExistingPresence = PresenceCache.Find(UserLive);
	auto& PresenceToUpdate = ExistingPresence ? *ExistingPresence : PresenceCache.Add(UserLive, MakeShareable(new FOnlineUserPresence));

	switch (Record->UserState)
	{
	case UserPresenceState::Online:
		PresenceToUpdate->bIsOnline = true;
		PresenceToUpdate->bIsPlaying = true;
		PresenceToUpdate->Status.State = EOnlinePresenceState::Online;
		break;

	case UserPresenceState::Away:
		PresenceToUpdate->bIsOnline = true;
		PresenceToUpdate->bIsPlaying = true;
		PresenceToUpdate->Status.State = EOnlinePresenceState::Away;
		break;

	case UserPresenceState::Offline:
		PresenceToUpdate->bIsOnline = false;
		PresenceToUpdate->bIsPlaying = false;
		PresenceToUpdate->Status.State = EOnlinePresenceState::Offline;
		break;

	case UserPresenceState::Unknown:
	default:
		break;
	}

	PresenceToUpdate->bIsPlayingThisGame = false;
	PresenceToUpdate->Status.Properties.Empty();
	if (Record->PresenceDeviceRecords->Size > 0)
	{
		check(LiveSubsystem);
		Microsoft::Xbox::Services::XboxLiveAppConfiguration^ AppConfig = LiveSubsystem->GetApplicationConfig();
		check(AppConfig != nullptr);

		PresenceDeviceRecord ^PopulateBasedOnDeviceRecord = Record->PresenceDeviceRecords->GetAt(0);
		PresenceTitleRecord ^PopulateBasedOnTitleRecord = nullptr;
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

		PresenceToUpdate->Status.Properties.Add(DefaultPlatformKey, PopulateBasedOnDeviceRecord->DeviceType.ToString()->Data());

		if (PopulateBasedOnTitleRecord != nullptr)
		{
			PresenceToUpdate->bIsPlayingThisGame = PopulateBasedOnTitleRecord->TitleId == AppConfig->TitleId;

			PresenceToUpdate->Status.Properties.Add(DefaultAppIdKey, PopulateBasedOnTitleRecord->TitleName->Data());

			// Use the Live Presence string as UE's StatusStr (consistent with other online platforms)
			PresenceToUpdate->Status.StatusStr = PopulateBasedOnTitleRecord->Presence->Data();

			// Also set the same string under the DefaultPresenceKey (consistent with original OSSLive behavior)
			PresenceToUpdate->Status.Properties.Add(DefaultPresenceKey, PresenceToUpdate->Status.StatusStr);
		}
	}

	return PresenceToUpdate;
}

TSharedRef<FOnlineUserPresence> FOnlinePresenceLive::CachePresenceFromLive(const FUniqueNetIdLive& UserLive, SocialManagerPresenceRecord ^Record)
{
	auto ExistingPresence = PresenceCache.Find(UserLive);
	auto& PresenceToUpdate = ExistingPresence ? *ExistingPresence : PresenceCache.Add(UserLive, MakeShareable(new FOnlineUserPresence));

	switch (Record->UserState)
	{
	case UserPresenceState::Online:
		PresenceToUpdate->bIsOnline = true;
		PresenceToUpdate->bIsPlaying = true;
		PresenceToUpdate->Status.State = EOnlinePresenceState::Online;
		break;

	case UserPresenceState::Away:
		PresenceToUpdate->bIsOnline = true;
		PresenceToUpdate->bIsPlaying = true;
		PresenceToUpdate->Status.State = EOnlinePresenceState::Away;
		break;

	case UserPresenceState::Offline:
		PresenceToUpdate->bIsOnline = false;
		PresenceToUpdate->bIsPlaying = false;
		PresenceToUpdate->Status.State = EOnlinePresenceState::Offline;
		break;

	case UserPresenceState::Unknown:
	default:
		break;
	}

	check(LiveSubsystem);
	Microsoft::Xbox::Services::XboxLiveAppConfiguration^ AppConfig = LiveSubsystem->GetApplicationConfig();
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

	PresenceToUpdate->bIsPlayingThisGame = false;
	PresenceToUpdate->Status.Properties.Empty();
	if (PopulateBasedOnTitleRecord != nullptr)
	{
		PresenceToUpdate->Status.Properties.Add(DefaultPlatformKey, PopulateBasedOnTitleRecord->DeviceType.ToString()->Data());

		PresenceToUpdate->bIsPlayingThisGame = PopulateBasedOnTitleRecord->TitleId == AppConfig->TitleId;

		// Note: localized game name not available via social manager version of presence record

		// Use the Live Presence string as UE's StatusStr (consistent with other online platforms)
		PresenceToUpdate->Status.StatusStr = PopulateBasedOnTitleRecord->PresenceText->Data();

		// Also set the same string under the DefaultPresenceKey (consistent with original OSSLive behavior)
		PresenceToUpdate->Status.Properties.Add(DefaultPresenceKey, PresenceToUpdate->Status.StatusStr);
	}

	return PresenceToUpdate;
}
// @ATG_CHANGE : END
