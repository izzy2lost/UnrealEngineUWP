// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveSendSessionInviteToFriends.h"
#include "OnlineSubsystemLive.h"

#include <collection.h>

FOnlineAsyncTaskLiveSendSessionInviteToFriends::FOnlineAsyncTaskLiveSendSessionInviteToFriends(FOnlineSubsystemLive* InLiveSubsystem,
																							   Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
																							   Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference,
																							   Windows::Foundation::Collections::IVectorView<Platform::String^>^ InFriendsToInvite)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLiveContext)
	, SessionReference(InSessionReference)
	, FriendsToInvite(InFriendsToInvite)
{
}

Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Platform::String^>^>^ FOnlineAsyncTaskLiveSendSessionInviteToFriends::CreateOperation()
{
	try
	{
		return LiveContext->MultiplayerService->SendInvitesAsync(SessionReference, FriendsToInvite, Subsystem->TitleId);
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error starting SendSessionInvitesToFriends, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
		return nullptr;
	}
}

bool FOnlineAsyncTaskLiveSendSessionInviteToFriends::ProcessResult(const Concurrency::task<Windows::Foundation::Collections::IVectorView<Platform::String^>^>& CompletedTask)
{
	try
	{
		Windows::Foundation::Collections::IVectorView<Platform::String^>^ Result = CompletedTask.get();
		UE_LOG_ONLINE(Verbose, TEXT("SendSessionInvitesToFriends Success"));
		return true;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error SendSessionInvitesToFriends, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
		return false;
	}
}
