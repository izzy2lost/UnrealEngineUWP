// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../OnlineAsyncTaskManagerLive.h"
#include "../OnlineSubsystemLiveTypes.h"

class FOnlineSubsystemLive;

/**
 * Async Task to send a session invite to friends
 */
class FOnlineAsyncTaskLiveSendSessionInviteToFriends
	: public FOnlineAsyncTaskConcurrencyLive<Windows::Foundation::Collections::IVectorView<Platform::String^>^>
{
public:
	FOnlineAsyncTaskLiveSendSessionInviteToFriends(FOnlineSubsystemLive* InLiveSubsystem,
												   Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
												   Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference,
												   Windows::Foundation::Collections::IVectorView<Platform::String^>^ InFriendsToInvite);
	virtual ~FOnlineAsyncTaskLiveSendSessionInviteToFriends() = default;

	//~ Begin FOnlineAsyncItem Interface
	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskLiveSendSessionInviteToFriends"); }

	virtual Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Platform::String^>^>^ CreateOperation() override;
	virtual bool ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Platform::String^>^>& CompletedTask) override;

protected:
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference;
	Windows::Foundation::Collections::IVectorView<Platform::String^>^ FriendsToInvite;
};