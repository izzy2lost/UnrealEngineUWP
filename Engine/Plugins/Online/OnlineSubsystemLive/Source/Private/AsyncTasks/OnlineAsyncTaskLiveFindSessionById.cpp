// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveFindSessionById.h"
#include "OnlineSubsystemLive.h"

using Microsoft::Xbox::Services::Multiplayer::MultiplayerSession;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference;
using Microsoft::Xbox::Services::XboxLiveContext;
using Windows::Foundation::Collections::IVectorView;
using Windows::Foundation::IAsyncOperation;

FOnlineAsyncTaskLiveFindSessionById::FOnlineAsyncTaskLiveFindSessionById(FOnlineSubsystemLive* InLiveSubsystem,
																		 Microsoft::Xbox::Services::XboxLiveContext^ InLiveContext,
																		 const int32 InLocalUserNum,
																		 FString&& InSessionIdString,
																		 const FOnSingleSessionResultCompleteDelegate& InDelegate)
	: FOnlineAsyncTaskConcurrencyLive(InLiveSubsystem, InLiveContext)
	, LocalUserNum(InLocalUserNum)
	, SessionIdString(MoveTemp(InSessionIdString))
	, Delegate(InDelegate)
	, OnlineError(false)
{
}

Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^>^ FOnlineAsyncTaskLiveFindSessionById::CreateOperation()
{
	UE_LOG_ONLINE(Verbose, TEXT("Attempting to find Session by Id %s"), *SessionIdString);
	try
	{
		MultiplayerSessionReference^ SessionRef = MultiplayerSessionReference::ParseFromUriPath(ref new Platform::String(*SessionIdString));

		IAsyncOperation<MultiplayerSession^>^ AsyncTask = LiveContext->MultiplayerService->GetCurrentSessionAsync(SessionRef);
		return AsyncTask;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error starting FindSessionById query, error: (0x%0.8X) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveFindSessionById::ProcessResult(const Concurrency::task<MultiplayerSession^>& CompletedTask)
{
	try
	{
		MultiplayerSession^ Session = CompletedTask.get();

		Platform::String^ HostDisplayName = nullptr;
		SearchResult = FOnlineSessionLive::CreateSearchResultFromSession(Session, HostDisplayName, LiveContext);

		OnlineError.bSucceeded = true;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error during FindSessionById, error: (0x%0.8X) %s."), Ex->HResult, Ex->ToString()->Data());
	}

	return OnlineError.bSucceeded;
}

void FOnlineAsyncTaskLiveFindSessionById::TriggerDelegates()
{
	Delegate.ExecuteIfBound(LocalUserNum, OnlineError.bSucceeded, SearchResult);
}
