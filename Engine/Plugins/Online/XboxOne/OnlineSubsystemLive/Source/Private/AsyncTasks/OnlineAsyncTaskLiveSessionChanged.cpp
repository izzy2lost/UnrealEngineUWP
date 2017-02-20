// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveSessionChanged.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"

using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Windows::Foundation;
using namespace concurrency;

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveSessionChanged::FOnlineAsyncTaskLiveSessionChanged(
	class FOnlineSubsystemLive* InLiveSubsystem, 
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference,
	FString InChangeBranch,
	uint64 InChangeNumber)
	: FOnlineAsyncTaskLive( InLiveSubsystem, INDEX_NONE )
	, LiveSessionReference( InSessionReference )
	, UpdatedLiveSession( nullptr )
	, ChangeBranch( InChangeBranch )
	, ChangeNumber( InChangeNumber )
	, bShouldTriggerDelegates( false )
{	
	SessionName = GetSessionNameForLiveSessionRef(LiveSessionReference);
	if (SessionName.IsNone())
	{
		UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start: couldn't find the existing session or match ticket."));
		OnFailed();
		return;
	}

	CachedLiveSession = LiveSubsystem->GetLastDiffedSession(SessionName);
	if (!CachedLiveSession)
	{
		UE_LOG(LogOnline, Error, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start: Couldn't get last diffed session."));
		OnFailed();
		return;
	}

	LiveContext = LiveSubsystem->GetLiveContext(CachedLiveSession);
	if (!LiveContext)
	{
		UE_LOG(LogOnline, Error, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start: Couldn't get XboxLiveContext for session."));
		OnFailed();
		return;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveSessionChanged::~FOnlineAsyncTaskLiveSessionChanged()
{
}

void FOnlineAsyncTaskLiveSessionChanged::OnFailed()
{
	bWasSuccessful = false;
	bIsComplete = true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void FOnlineAsyncTaskLiveSessionChanged::Start() 
{
	if (bIsComplete)
	{
		return;		// something failed in the constructor
	}

	UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start - Branch: %s, ChangeNumber: %u"), *ChangeBranch, ChangeNumber);

	if (LiveSubsystem->GetSessionMessageRouter()->GetLastProcessedChangeNumber(ChangeBranch) >= ChangeNumber)
	{
		UE_LOG(LogOnlineSubsystemLive, Log, TEXT("  Change was already handled, skipping session update"));
		bWasSuccessful = true;
		bIsComplete = true;
		return;
	}

	UE_LOG(LogOnlineSubsystemLive, Log, TEXT("  Getting updated session from Live"));

	create_task(LiveContext->MultiplayerService->GetCurrentSessionAsync(LiveSessionReference))
		.then([this](concurrency::task<MultiplayerSession^> Task)
	{
		try
		{
			UpdatedLiveSession = Task.get();
			LiveSubsystem->GetSessionMessageRouter()->SetLastProcessedChangeNumber(UpdatedLiveSession->Branch->Data(), UpdatedLiveSession->ChangeNumber);

			UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FOnlineAsyncTaskLiveSessionChanged::Start - Got session at: Branch: %s, ChangeNumber: %u"), UpdatedLiveSession->Branch->Data(), UpdatedLiveSession->ChangeNumber);

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

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void FOnlineAsyncTaskLiveSessionChanged::Finalize() 
{
	if (bWasSuccessful && UpdatedLiveSession)
	{
		//Get this again in case it changed before this ran
		if (LiveSessionReference = GetLiveSessionRefForSessionName(SessionName))
		{
			if (FOnlineSubsystemLive::AreSessionReferencesEqual(LiveSessionReference, UpdatedLiveSession->SessionReference))
			{				
				CachedLiveSession = LiveSubsystem->GetLastDiffedSession(SessionName);

				Diff = MultiplayerSession::CompareMultiplayerSessions(UpdatedLiveSession, CachedLiveSession);

				LiveSubsystem->RefreshLiveInfo(SessionName, UpdatedLiveSession);
				LiveSubsystem->SetLastDiffedSession(SessionName, UpdatedLiveSession);

				bShouldTriggerDelegates = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void FOnlineAsyncTaskLiveSessionChanged::TriggerDelegates()
{				
	if (bShouldTriggerDelegates)
	{
		LiveSubsystem->GetSessionMessageRouter()->TriggerOnSessionChangedDelegates(LiveSessionReference, SessionName, Diff);
	}
}

//-----------------------------------------------------------------------------
// Conversion helpers
//-----------------------------------------------------------------------------

FName FOnlineAsyncTaskLiveSessionChanged::GetSessionNameForLiveSessionRef(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LiveSessionRef)
{
	FName EmptyName;

	if (FNamedOnlineSession* FoundSession = Subsystem->GetSessionInterfaceLive()->GetNamedSessionForLiveSessionRef(LiveSessionRef))
	{
		return FoundSession->SessionName;
	}

	FOnlineMatchTicketInfoPtr MatchTicket = Subsystem->GetMatchmakingInterfaceLive()->GetMatchTicketForLiveSessionRef(LiveSessionRef);
	if (MatchTicket.IsValid())
	{
		return MatchTicket->SessionName;
	}

	return EmptyName;
}

Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ FOnlineAsyncTaskLiveSessionChanged::GetLiveSessionRefForSessionName(const FName& SessionName)
{
	if (FNamedOnlineSession* NamedSession = Subsystem->GetSessionInterfaceLive()->GetNamedSession(SessionName))
	{
		auto LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
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

//------------------------------- End of file ---------------------------------
