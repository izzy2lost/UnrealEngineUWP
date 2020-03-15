// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlineUserInterfaceLive.h"
#include "OnlineError.h"

class FOnlineSubsystemLive;

/**
 * Async Task to query the account details of a user
 */
class FOnlineAsyncTaskLiveQueryUsers
	: public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^>
{
public:
	FOnlineAsyncTaskLiveQueryUsers(FOnlineSubsystemLive* InLiveSubsystem,
								   Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
								   const TArray<TSharedRef<const FUniqueNetId>>& InUsersToQuery,
								   int32 InLocalUserNum,
								   const FOnQueryUserInfoComplete& InDelegate);
	virtual ~FOnlineAsyncTaskLiveQueryUsers() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryUsers"); }

	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^>& CompletedTask) override;

	virtual void Finalize();
	virtual void TriggerDelegates();

protected:
	TArray<TSharedRef<const FUniqueNetId>> UsersToQuery;
	int32 LocalUserNum;
	FOnQueryUserInfoComplete Delegate;
	FOnlineError OnlineError;
	TMap<FUniqueNetIdLive, TSharedRef<FOnlineUserInfoLive>> UserInfoMap;
};