// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveUnregisterLocalUser.h"
#include "OnlineSubsystemLive.h"

using namespace Microsoft::Xbox::Services::Multiplayer;

FOnlineAsyncTaskLiveUnregisterLocalUser::FOnlineAsyncTaskLiveUnregisterLocalUser(
	FName InSessionName,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	const FUniqueNetIdLive& InUserId,
	const FOnUnregisterLocalPlayerCompleteDelegate& InDelegate)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InContext, InSubsystem, DefaultRetryCount)
	, UserId(InUserId)
	, Delegate(InDelegate)
{
}

bool FOnlineAsyncTaskLiveUnregisterLocalUser::UpdateSession(MultiplayerSession^ Session)
{
	Session->Leave();
	return true;
}

void FOnlineAsyncTaskLiveUnregisterLocalUser::TriggerDelegates()
{
	Delegate.ExecuteIfBound(UserId, bWasSuccessful);
}
