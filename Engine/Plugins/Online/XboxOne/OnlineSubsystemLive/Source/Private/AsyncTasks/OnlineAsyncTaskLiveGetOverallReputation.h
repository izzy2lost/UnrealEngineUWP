// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "OnlineError.h"

typedef TMap<const FUniqueNetIdLive, bool> UserReputationMap;
DECLARE_DELEGATE_OneParam(FOnGetOverallReputationCompleteDelegate, const UserReputationMap& /* UsersWithBadReputations */);

class FOnlineAsyncTaskLiveGetOverallReputation
	: public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult^>^>
{
public:

	FOnlineAsyncTaskLiveGetOverallReputation(FOnlineSubsystemLive* const InLiveSubsystem
		, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext
		, TArray<TSharedRef<const FUniqueNetIdLive> >&& InUserIDs
		, const FOnGetOverallReputationCompleteDelegate& InCompletionDelegate
	);

	FOnlineAsyncTaskLiveGetOverallReputation(FOnlineSubsystemLive* const InLiveSubsystem
		, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext
		, const TSharedRef<const FUniqueNetIdLive>& InUserID
		, const FOnGetOverallReputationCompleteDelegate& InCompletionDelegate
	);

	virtual FString ToString() const override;

private:
	/**
	* Handles creating an AsyncOperation.  The task should be created and returned in this method
	*
	* @Return The AsyncOperation to run, or nullptr if there was a failure
	*/
	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult^>^>^ CreateOperation();

	/**
	* Handles processing of an async result.  Any processed data should be stored on the task until Finalize is called on the Game thread.
	*
	* @Param CompletedTask The finished task to call get() on
	* @Return Whether or not our task was successful and is stored in bWasSuccessful
	*/
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult^>^>& CompletedTask);

	virtual void TriggerDelegates() override;

private:
	FOnGetOverallReputationCompleteDelegate CompletionDelegate;

	TArray<TSharedRef<const FUniqueNetIdLive> > UserIds;
	UserReputationMap UsersWithBadReputations;
};