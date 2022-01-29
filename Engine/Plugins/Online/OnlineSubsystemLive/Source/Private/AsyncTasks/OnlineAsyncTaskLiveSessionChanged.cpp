// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveSessionChanged.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"

using Microsoft::Xbox::Services::Multiplayer::MultiplayerSession;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference;

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveSessionChanged::FOnlineAsyncTaskLiveSessionChanged(FOnlineSubsystemLive* InLiveSubsystem,
																	   MultiplayerSessionReference^ InSessionReference,
																	   FString InChangeBranch,
																	   uint64 InChangeNumber)
	: FOnlineAsyncTaskLive(InLiveSubsystem, INDEX_NONE)
	, LiveSessionReference(InSessionReference)
	, UpdatedLiveSession(nullptr)
	, ChangeBranch(InChangeBranch)
	, ChangeNumber(InChangeNumber)
	, bShouldTriggerDelegates(false)
{
	SessionName = GetSessionNameForLiveSessionRef(LiveSessionReference);
	if (SessionName.IsNone())
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start: couldn't find the existing session or match ticket."));
		OnFailed();
		return;
	}

	CachedLiveSession = Subsystem->GetLastDiffedSession(SessionName);
	if (!CachedLiveSession)
	{
		UE_LOG(LogOnline, Error, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start: Couldn't get last diffed session."));
		OnFailed();
		return;
	}

	LiveContext = Subsystem->GetLiveContext(CachedLiveSession);
	if (!LiveContext)
	{
		UE_LOG(LogOnline, Error, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start: Couldn't get XboxLiveContext for session."));
		OnFailed();
		return;
	}
}

void FOnlineAsyncTaskLiveSessionChanged::OnFailed()
{
	bWasSuccessful = false;
	bIsComplete = true;
}

void FOnlineAsyncTaskLiveSessionChanged::Initialize()
{
	if (bIsComplete)
	{
		// something failed in the constructor
		return;
	}

	UE_LOG_ONLINE(Log, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start - Branch: %s, ChangeNumber: %u"), *ChangeBranch, ChangeNumber);

	if (Subsystem->GetSessionMessageRouter()->GetLastProcessedChangeNumber(ChangeBranch) >= ChangeNumber)
	{
		UE_LOG_ONLINE(Log, TEXT("  Change was already handled, skipping session update"));
		bWasSuccessful = true;
		bIsComplete = true;
		return;
	}

	UE_LOG_ONLINE(Log, TEXT("  Getting updated session from Live"));

	Concurrency::create_task(LiveContext->MultiplayerService->GetCurrentSessionAsync(LiveSessionReference))
		.then([this](Concurrency::task<MultiplayerSession^> Task)
	{
		try
		{
			UpdatedLiveSession = Task.get();
			Subsystem->GetSessionMessageRouter()->SetLastProcessedChangeNumber(UpdatedLiveSession->Branch->Data(), UpdatedLiveSession->ChangeNumber);

			UE_LOG_ONLINE(Log, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start - Got session at: Branch: %s, ChangeNumber: %u"), UpdatedLiveSession->Branch->Data(), UpdatedLiveSession->ChangeNumber);

			bWasSuccessful = true;
			bIsComplete = true;
		}
		catch (Platform::Exception^ ex)
		{
			UE_LOG(LogOnline, Error, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start: error getting updated game session: 0x%0.8X"), ex->HResult);
			OnFailed();
		}
	});
}

void FOnlineAsyncTaskLiveSessionChanged::Finalize()
{
	if (bWasSuccessful && UpdatedLiveSession)
	{
		//Get this again in case it changed before this ran
		if (LiveSessionReference = GetLiveSessionRefForSessionName(SessionName))
		{
			if (FOnlineSubsystemLive::AreSessionReferencesEqual(LiveSessionReference, UpdatedLiveSession->SessionReference))
			{
				CachedLiveSession = Subsystem->GetLastDiffedSession(SessionName);

				Diff = MultiplayerSession::CompareMultiplayerSessions(UpdatedLiveSession, CachedLiveSession);

				Subsystem->RefreshLiveInfo(SessionName, UpdatedLiveSession);
				Subsystem->SetLastDiffedSession(SessionName, UpdatedLiveSession);

				bShouldTriggerDelegates = true;
			}
		}

		FOnlineSessionLivePtr SessionInt = Subsystem->GetSessionInterfaceLive();
		if (SessionInt.IsValid())
		{
			SessionInt->UpdateSessionChangedStats();
		}
	}
}

void FOnlineAsyncTaskLiveSessionChanged::TriggerDelegates()
{
	if (bShouldTriggerDelegates)
	{
		Subsystem->GetSessionMessageRouter()->TriggerOnSessionChangedDelegates(LiveSessionReference, SessionName, Diff);
	}
}

FName FOnlineAsyncTaskLiveSessionChanged::GetSessionNameForLiveSessionRef(MultiplayerSessionReference^ LiveSessionRef)
{
	if (FNamedOnlineSession* FoundSession = Subsystem->GetSessionInterfaceLive()->GetNamedSessionForLiveSessionRef(LiveSessionRef))
	{
		return FoundSession->SessionName;
	}

	FOnlineMatchTicketInfoPtr MatchTicket = Subsystem->GetMatchmakingInterfaceLive()->GetMatchTicketForLiveSessionRef(LiveSessionRef);
	if (MatchTicket.IsValid())
	{
		return MatchTicket->SessionName;
	}

	return NAME_None;
}

MultiplayerSessionReference^ FOnlineAsyncTaskLiveSessionChanged::GetLiveSessionRefForSessionName(const FName& SessionName)
{
	if (FNamedOnlineSession* NamedSession = Subsystem->GetSessionInterfaceLive()->GetNamedSession(SessionName))
	{
		FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
		if (LiveInfo.IsValid())
		{
			return LiveInfo->GetLiveMultiplayerSessionRef();
		}
	}

	FOnlineMatchTicketInfoPtr MatchTicket;
	Subsystem->GetMatchmakingInterfaceLive()->GetMatchmakingTicket(SessionName, MatchTicket);
	if (MatchTicket.IsValid())
	{
		return MatchTicket->GetLiveSessionRef();
	}

	return nullptr;
}
