// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveSetSessionActivity.h"
#include "OnlineSubsystemLive.h"

FOnlineAsyncTaskLiveSetSessionActivity::FOnlineAsyncTaskLiveSetSessionActivity(FOnlineSubsystemLive* InLiveSubsystem,
																			   Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
																			   Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLiveContext)
	, SessionReference(InSessionReference)
{

}

Windows::Foundation::IAsyncAction^ FOnlineAsyncTaskLiveSetSessionActivity::CreateOperation()
{
	try
	{
		return LiveContext->MultiplayerService->SetActivityAsync(SessionReference);
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error starting SetActivityAsync, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
		return nullptr;
	}
}

bool FOnlineAsyncTaskLiveSetSessionActivity::ProcessResult(const Concurrency::task<void>& CompletedTask)
{
	try
	{
		CompletedTask.get();

		// We succeeded if CompletedTask didn't throw
		UE_LOG_ONLINE(Verbose, TEXT("Successfully updated Multiplayer Session Activity for player %ls"), LiveContext->User->XboxUserId->Data());
		return true;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to update Multiplayer Session Activity for player %ls, error: (0x%0.8X) %ls."), LiveContext->User->XboxUserId->Data(), Ex->HResult, Ex->ToString()->Data());
		return false;
	}
}
