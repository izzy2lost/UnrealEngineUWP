// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskLiveSafeWriteSession.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "../OnlineSubsystemLiveTypes.h"

/** 
 * Async task used to have another local user Join() a Live session.
 */
class FOnlineAsyncTaskLiveRegisterLocalUser : public FOnlineAsyncTaskLiveSafeWriteSession
{
public:
	FOnlineAsyncTaskLiveRegisterLocalUser(
			FName InSessionName,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			FOnlineSubsystemLive* InSubsystem,
			const FUniqueNetIdLive& InUserId,
			const FOnRegisterLocalPlayerCompleteDelegate& InDelegate);

	// Allow initialization group to be specified, so QoS knows which users are on the same console
	FOnlineAsyncTaskLiveRegisterLocalUser(
			FName InSessionName,
			Microsoft::Xbox::Services::XboxLiveContext^ InContext,
			FOnlineSubsystemLive* InSubsystem,
			const FUniqueNetIdLive& InUserId,
			const FOnRegisterLocalPlayerCompleteDelegate& InDelegate,
			Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^>^ InInitializationGroup);
	
	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("RegisterLocalUser"); }
	virtual void TriggerDelegates() override;
	virtual void Finalize() override;

private:

	// FOnlineAsyncTaskLiveSafeWriteSession
	virtual bool UpdateSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session);

	FUniqueNetIdLive UserId;
	FOnRegisterLocalPlayerCompleteDelegate Delegate;

	EOnJoinSessionCompleteResult::Type Result;

	// Allow initialization group to be specified, so QoS knows which users are on the same console
	Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember^>^ InitializationGroup;
};
