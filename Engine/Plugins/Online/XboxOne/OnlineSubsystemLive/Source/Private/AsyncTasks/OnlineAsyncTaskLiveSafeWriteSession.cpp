// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveSafeWriteSession.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace concurrency;


FOnlineAsyncTaskLiveSafeWriteSession::FOnlineAsyncTaskLiveSafeWriteSession(
	FName InSessionName,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	int RetryCount)
	: FOnlineAsyncTaskLive(InSubsystem, 0)
	, LiveContext(InContext)
	, LiveSession(nullptr)
	, SessionName(InSessionName)
	, RetryCount(RetryCount)
{
	check(Subsystem);
	bWasSuccessful = true;

	auto NamedSession = Subsystem->GetSessionInterfaceLive()->GetNamedSession(SessionName);

	auto LiveInfo = NamedSession ? StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo) : nullptr;
	FOnlineMatchTicketInfoPtr MatchTicket;
	Subsystem->GetMatchmakingInterfaceLive()->GetMatchmakingTicket(SessionName, MatchTicket);
	if (LiveInfo.IsValid())
	{
		SessionReference = LiveInfo->GetLiveMultiplayerSessionRef();
	}

	// Use match or game session as appropriate
	else if (MatchTicket.IsValid())
	{
		SessionReference = MatchTicket->GetLiveSessionRef();
	}
	else
	{
		OnFailed();
	}
}

FOnlineAsyncTaskLiveSafeWriteSession::FOnlineAsyncTaskLiveSafeWriteSession(
	FName InSessionName,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	int RetryCount)
	: FOnlineAsyncTaskLive(InSubsystem, 0)
	, SessionReference(InSessionReference)
	, LiveContext(InContext)
	, LiveSession(nullptr)
	, SessionName(InSessionName)
	, RetryCount(RetryCount)
{
}

void FOnlineAsyncTaskLiveSafeWriteSession::OnFailed()
{
	bWasSuccessful = false;
	bIsComplete = true;
}

void FOnlineAsyncTaskLiveSafeWriteSession::Retry(bool bGetSession)
{
	if ( RetryCount <= 0 )
	{
		OnFailed();
		return;
	}
	--RetryCount;

	if (bGetSession)
	{
		auto GetSessionOp = LiveContext->MultiplayerService->GetCurrentSessionAsync(SessionReference);
		create_task(GetSessionOp).then([this](task<MultiplayerSession^> SessionTask)
		{
			try
			{
				LiveSession = SessionTask.get();

				TryWriteSession();
			}
			catch(Platform::COMException^ Ex)
			{
				if (Ex->HResult == HTTP_E_STATUS_NOT_FOUND)
				{
					Retry(true);
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("Failed to get session from session reference."));
					OnFailed();
				}
			}
		});
	}
	else
	{
		TryWriteSession();
	}
}

void FOnlineAsyncTaskLiveSafeWriteSession::TryWriteSession()
{
	if (LiveSession == nullptr)
	{
		OnFailed();
		return;
	}

	bUpdatedSession = UpdateSession(LiveSession);

	if (!bUpdatedSession)
	{
		// Subclass decided not to change the session, we're done here.
		bWasSuccessful = true;
		bIsComplete = true;
		return;
	}

	auto WriteOp = LiveContext->MultiplayerService->TryWriteSessionAsync(LiveSession, MultiplayerSessionWriteMode::SynchronizedUpdate);
	create_task(WriteOp).then([this](task<WriteSessionResult^> WriteTask)
	{
		try
		{
			WriteSessionResult^ Result = WriteTask.get();
			LiveSession = Result->Session;

			if (Result->Succeeded)
			{
				bWasSuccessful = true;
				bIsComplete = true;
			}
			else if (Result->Status == WriteSessionStatus::OutOfSync)
			{
				Retry(false);
			}
			else
			{
				OnFailed();
			}
		}
		catch(Platform::Exception^ Ex)
		{
			OnFailed();
		}
	});
}

void FOnlineAsyncTaskLiveSafeWriteSession::Finalize()
{
	Subsystem->RefreshLiveInfo(SessionName, LiveSession);
}

void FOnlineAsyncTaskLiveSafeWriteSession::Initialize()
{
	Retry(true);
}
