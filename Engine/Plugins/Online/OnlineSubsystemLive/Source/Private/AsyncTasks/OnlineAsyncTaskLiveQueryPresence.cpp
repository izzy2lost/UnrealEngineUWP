// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveQueryPresence.h"
#include "OnlineSubsystemLive.h"

using namespace Microsoft::Xbox::Services::Presence;
using namespace Microsoft::Xbox::Services::UserStatistics;
using Windows::Foundation::Collections::IVectorView;

void FOnlineAsyncTaskLiveQueryPresence::Initialize()
{
	Platform::String^ UserToQueryXuid = ref new Platform::String(*User.UniqueNetIdStr);

	try
	{
		Windows::Foundation::IAsyncOperation<PresenceRecord^>^ GetPresenceOp = LiveContext->PresenceService->GetPresenceAsync(UserToQueryXuid);
		PresenceTask = concurrency::create_task(GetPresenceOp);

		// Also query stats that have been configured to use as presence keys.
		IVectorView<Platform::String^>^ StatNameVectorView = FOnlinePresenceLive::GetConfiguredPresenceStatNames();
		if (StatNameVectorView->Size > 0)
		{
			try
			{
				Windows::Foundation::IAsyncOperation<UserStatisticsResult^>^ StatsOp = LiveContext->UserStatisticsService->GetSingleUserStatisticsAsync(
				UserToQueryXuid,
				// @ATG_CHANGE : BEGIN uwp support
				LiveContext->AppConfig->ServiceConfigurationId,
				// @ATG_CHANGE : END
				StatNameVectorView);

				StatsTask = Concurrency::create_task(StatsOp);
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskLiveQueryPresence::Initialize: %s exception from GetSingleUserStatisticsAsync, with code 0x%08X and reason '%s'."), Ex->GetType()->ToString()->Data(), *ToString(), Ex->HResult, Ex->Message->Data());
				bIsComplete = true;
				bWasSuccessful = false;
			}
			
		}
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskLiveQueryPresence::Initialize: %s exception from GetPresenceAsync, with code 0x%08X and reason '%s'."), Ex->GetType()->ToString()->Data(), *ToString(), Ex->HResult, Ex->Message->Data());
		bIsComplete = true;
		bWasSuccessful = false;
	}
}

void FOnlineAsyncTaskLiveQueryPresence::Tick()
{
	if (bIsComplete)
	{
		return;
	}

	if (PresenceTask.is_done() && (!StatsTask.IsSet() || StatsTask->is_done()))
	{
		bool bPresenceSuccessful = false;
		bool bStatsSuccessful = !StatsTask.IsSet();

		try
		{
			PresenceResult = PresenceTask.get();
			bPresenceSuccessful = true;
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("The get presence task failed. Exception: %ls."), Ex->ToString()->Data());
		}

		if (StatsTask.IsSet())
		{
			try
			{
				StatsResult = StatsTask->get();
				bStatsSuccessful = true;
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("The get presence task failed (stats). Exception: %ls."), Ex->ToString()->Data());
			}
		}

		bIsComplete = true;
		bWasSuccessful = bPresenceSuccessful && bStatsSuccessful;
	}
}

void FOnlineAsyncTaskLiveQueryPresence::Finalize()
{
	FOnlineAsyncTaskBasic::Finalize();

	if(!Subsystem || !Subsystem->GetPresenceLive().IsValid())
	{
		bWasSuccessful = false;
		return;
	}

	if(!PresenceResult || !PresenceResult->PresenceDeviceRecords)
	{
		bWasSuccessful = false;
		return;
	}

	FUniqueNetIdLive UserLive(PresenceResult->XboxUserId->Data());
	TSharedRef<FOnlineUserPresenceLive> NewPresence = MakeShared<FOnlineUserPresenceLive>(PresenceResult);
	NewPresence->SetStatusPropertiesFromStatistics(StatsResult);
	Subsystem->GetPresenceLive()->PresenceCache.Emplace(MoveTemp(UserLive), NewPresence);
}

void FOnlineAsyncTaskLiveQueryPresence::TriggerDelegates()
{
	FOnlineAsyncTaskBasic::TriggerDelegates();

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
