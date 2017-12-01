// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlineFriendsInterfaceLive.h"

/**
 * Async task to query our avoid list
 */
class FOnlineAsyncTaskLiveQueryAvoidList
	: public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Platform::String^>^>
{
public:
	FOnlineAsyncTaskLiveQueryAvoidList(FOnlineSubsystemLive* const InLiveInterface, Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext, const FUniqueNetIdLive& InUserIdLive);
	virtual ~FOnlineAsyncTaskLiveQueryAvoidList() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveQueryAvoidList"); }

	// Starts in Game Thread
	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Platform::String^>^>^ CreateOperation() override;
	// Process in Online Thread
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Platform::String^>^>& CompletedTask) override;

	// Move results and trigger delegates in Game Thread
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

protected:
	FUniqueNetIdLive UserIdLive;

	TArray<TSharedRef<FOnlineBlockedPlayerLive>> AvoidList;
	FString OutError;
};