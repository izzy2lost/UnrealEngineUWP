// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"

class FOnlineSubsystemLive;

/**
 * Async Task to set the current session for a user
 */
class FOnlineAsyncTaskLiveSetSessionActivity
	: public FOnlineAsyncTaskConcurrencyLiveAsyncAction
{
public:
	FOnlineAsyncTaskLiveSetSessionActivity(FOnlineSubsystemLive* InLiveSubsystem,
										   Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
										   Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference);
	virtual ~FOnlineAsyncTaskLiveSetSessionActivity() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveSetSessionActivity"); }

	virtual Windows::Foundation::IAsyncAction^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<void>& CompletedTask) override;

protected:
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference;
};
