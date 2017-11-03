// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveGetOverallReputation.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "OnlineSubsystemLive.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Microsoft;
using namespace Windows::Foundation;
using namespace concurrency;
using namespace Microsoft::Xbox::Services::UserStatistics;

using Windows::Foundation::Collections::IVectorView;

FOnlineAsyncTaskLiveGetOverallReputation::FOnlineAsyncTaskLiveGetOverallReputation(
	FOnlineSubsystemLive* const InLiveSubsystem
	, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext
	, TArray<TSharedRef<const FUniqueNetIdLive> >&& InUserIDs
	, const FOnGetOverallReputationCompleteDelegate& InCompletionDelegate
)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLiveContext)
	, UserIds(MoveTemp(InUserIDs))
	, CompletionDelegate(InCompletionDelegate)
{

}

FOnlineAsyncTaskLiveGetOverallReputation::FOnlineAsyncTaskLiveGetOverallReputation(
	FOnlineSubsystemLive* const InLiveSubsystem,
	Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
	const TSharedRef<const FUniqueNetIdLive>& InUserID,
	const FOnGetOverallReputationCompleteDelegate& InCompletionDelegate
)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLiveContext)
	, CompletionDelegate(InCompletionDelegate)
{
	UserIds.Add(InUserID);
}

FString FOnlineAsyncTaskLiveGetOverallReputation::ToString() const
{
	return TEXT("GetOverallReputation task");
}

IAsyncOperation<IVectorView<UserStatisticsResult^>^>^ FOnlineAsyncTaskLiveGetOverallReputation::CreateOperation()
{
	Platform::Collections::Vector<Platform::String^> UserIDsToSend;

	for (const TSharedRef<const FUniqueNetIdLive>& UserId : UserIds)
	{
		Platform::String^ UserIdString = ref new Platform::String(*UserId->ToString());

		UserIDsToSend.Append(UserIdString);
	}

	Platform::Collections::Vector<Platform::String^> StatisticsToSearchFor;
	StatisticsToSearchFor.Append(ref new Platform::String(TEXT("OverallReputationIsBad")));
	try 
	{
		auto GetReputationOperation = LiveContext->UserStatisticsService->GetMultipleUserStatisticsAsync(UserIDsToSend.GetView(),
			ref new Platform::String(LIVE_GLOBAL_SCID),
			StatisticsToSearchFor.GetView()
		);

		return GetReputationOperation;
	}
	catch (Platform::COMException^ Ex)
	{
		UE_LOG(LogOnline, Warning, TEXT("FOnlineAsyncTaskLiveGetOverallReputation: GetMultipleUserStatisticsAsync: Failed to get statistics. HResult = 0x%0.8X"), Ex->HResult);
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveGetOverallReputation::ProcessResult(const Concurrency::task<IVectorView<UserStatisticsResult^>^>& CompletedTask)
{
	try
	{
		IVectorView<UserStatisticsResult^>^ UserStats = CompletedTask.get();
		for (uint32 UserStatIndex = 0; UserStatIndex < UserStats->Size; ++UserStatIndex)
		{
			UserStatisticsResult^ Stats = UserStats->GetAt(UserStatIndex);
			IVectorView<ServiceConfigurationStatistic^>^ ServiceStats = Stats->ServiceConfigurationStatistics;

			UE_LOG_ONLINE(Verbose, TEXT("Reputation: Found %d statistics for user %s"), ServiceStats->Size, Stats->XboxUserId->Data());

			bool bOverallReputationIsBad = false;

			for (uint32 ConfigStatIndex = 0; ConfigStatIndex < ServiceStats->Size; ++ConfigStatIndex)
			{
				ServiceConfigurationStatistic^ ConfigStat = ServiceStats->GetAt(ConfigStatIndex);

				UE_LOG_ONLINE(Verbose, TEXT("Reputation: stat %d: %s"), ConfigStatIndex, ConfigStat->ToString()->Data());
				UE_LOG_ONLINE(Verbose, TEXT("Reputation: Found %d config statistics in stat %d"), ConfigStat->Statistics->Size, ConfigStatIndex);

				for (uint32 StatIndex = 0; StatIndex < ConfigStat->Statistics->Size; ++StatIndex)
				{
					Statistic^ Stat = ConfigStat->Statistics->GetAt(StatIndex);

					FString StatName(Stat->StatisticName->Data());
					if (StatName.Equals(TEXT("OverallReputationIsBad")))
					{
						// the Value returned by the query should be either "0" or "1" for false or true, respectively
						FString StatValue(Stat->Value->Data());
						bOverallReputationIsBad = StatValue.ToBool();
					}
				}
			}
			FUniqueNetIdLive UserId(Stats->XboxUserId->Data());
			UsersWithBadReputations.Add(MoveTemp(UserId), bOverallReputationIsBad);
		}
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("GetMultipleUserStatisticsAsync failed with 0x%0.8X"), Ex->HResult);
		return false;
	}

	return true;
}

void FOnlineAsyncTaskLiveGetOverallReputation::TriggerDelegates()
{
	CompletionDelegate.ExecuteIfBound(UsersWithBadReputations);
}
