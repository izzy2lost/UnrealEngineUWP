// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveCancelMatchmaking.h"
#include "OnlineAsyncTaskLiveDestroySession.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"
#include "OnlineAsyncTaskLiveDestroySession.h"
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;

using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Windows::Foundation;
// @ATG_CHANGE : UWP LIVE support: Xbox header to pch

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveCancelMatchmaking::FOnlineAsyncTaskLiveCancelMatchmaking(
	class FOnlineSubsystemLive* InLiveSubsystem, 
	XboxLiveContext^ InUserContext,
	FName InSessionName,
	FOnlineMatchTicketInfoPtr InTicketInfo) 

	: FOnlineAsyncTaskLive( InLiveSubsystem, INDEX_NONE )
	, SessionName( InSessionName )
	, UserContext( InUserContext )
	, bCancelInProgress( false )
	, TicketInfo(InTicketInfo)
{
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveCancelMatchmaking::~FOnlineAsyncTaskLiveCancelMatchmaking()
{
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void FOnlineAsyncTaskLiveCancelMatchmaking::Initialize()
{
	if (TicketInfo.IsValid() == false)
	{
		bWasSuccessful = true;
		bIsComplete = true;
		return;
	}

	try
	{
		auto CancelMatchTicketOperation = UserContext->MatchmakingService->DeleteMatchTicketAsync(
			// @ATG_CHANGE : UWP Live Support - BEGIN
			UserContext->AppConfig->ServiceConfigurationId,
			// @ATG_CHANGE : UWP Live Support - BEGIN
			ref new Platform::String(*TicketInfo->HopperName),
			ref new Platform::String(*TicketInfo->TicketId));

		concurrency::create_task( CancelMatchTicketOperation )
			.then( [this] (concurrency::task<void> t)
		{
			try
			{
				t.get(); // if t.get() didn't throw, it succeeded

				Subsystem->GetMatchmakingInterfaceLive()->RemoveMatchmakingTicket(SessionName);

				UE_LOG(LogOnline, Log, TEXT("\nMatchmaking ticket cancelled. (%s:%s)"), *SessionName.ToString(), *TicketInfo->TicketId);
				bWasSuccessful = true;
				bIsComplete = true;
			}
			catch (Platform::Exception^ ex)
			{
				UE_LOG(LogOnline, Error, TEXT("\nDeleteMatchTicketAsync failed with 0x%0.8X"), ex->HResult);
				bWasSuccessful = false;
				bIsComplete = true;
			}
		});
	}
	catch (Platform::COMException^ Ex)
	{
		UE_LOG(LogOnline, Warning, TEXT("FOnlineAsyncTaskLiveCancelMatchmaking::CancelLiveTicket: Failed to delete ticket. HResult = 0x%0.8X"), Ex->HResult);
		bWasSuccessful = false;
		bIsComplete = true;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void FOnlineAsyncTaskLiveCancelMatchmaking::TriggerDelegates()
{
	Subsystem->GetMatchmakingInterfaceLive()->TriggerOnCancelMatchmakingCompleteDelegates(SessionName, bWasSuccessful);
}

//------------------------------- End of file ---------------------------------
