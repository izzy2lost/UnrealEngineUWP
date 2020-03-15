// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"
#include "../OnlineSessionInterfaceLive.h"
#include "OnlineError.h"

class FOnlineSubsystemLive;

/**
 * Async Task to find the session for a session id
 */
class FOnlineAsyncTaskLiveFindSessionById
	: public FOnlineAsyncTaskConcurrencyLive<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>
{
public:
	FOnlineAsyncTaskLiveFindSessionById(FOnlineSubsystemLive* InLiveSubsystem,
										Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
										const int32 InLocalUserNum,
										FString&& InSessionIdString,
										const FOnSingleSessionResultCompleteDelegate& InDelegate);

	virtual ~FOnlineAsyncTaskLiveFindSessionById() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveFindSessionById"); }

	virtual Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>& CompletedTask) override;

	virtual void TriggerDelegates();

protected:
	TArray<TSharedRef<const FUniqueNetId>> UsersToQuery;
	int32 LocalUserNum;
	FString SessionIdString;
	FOnSingleSessionResultCompleteDelegate Delegate;
	FOnlineError OnlineError;
	FOnlineSessionSearchResult SearchResult;
};