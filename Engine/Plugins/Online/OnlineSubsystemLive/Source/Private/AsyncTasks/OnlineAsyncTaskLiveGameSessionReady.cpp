// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineSubsystemLive.h"
#include "OnlineAsyncTaskLiveGameSessionReady.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "OnlineAsyncTaskLiveSetSessionActivity.h"

#include <collection.h>

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace concurrency;

using namespace Platform;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Windows::Xbox::System;


FOnlineAsyncTaskLiveGameSessionReady::FOnlineAsyncTaskLiveGameSessionReady(
	FOnlineSubsystemLive* InSubsystem,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FName InSessionName,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InGameSessionRef)
	: FOnlineAsyncTaskLive(InSubsystem, 0)
	, LiveContext(InContext)
	, SessionName(InSessionName)
	, GameSessionRef(InGameSessionRef)
{
}

void FOnlineAsyncTaskLiveGameSessionReady::Initialize()
{
	check(LiveContext != nullptr);

	auto asyncOp = LiveContext->MultiplayerService->GetCurrentSessionAsync(GameSessionRef);

	create_task(asyncOp)
		.then([this] (task<MultiplayerSession^> result)
	{
		try
		{
			auto session_ret = result.get();

			if ( session_ret != nullptr )
			{
				UE_LOG(LogOnline, Log, L"FOnlineAsyncTaskLiveGameSessionReady::Start - Session ready!");

				LiveSession = session_ret;

				const int UserIndex = 0;

				auto SessionInterface = StaticCastSharedPtr<FOnlineSessionLive>(Subsystem->GetSessionInterface());

				//. Mark the current user as active, or they will time out and be removed from the session
				SessionInterface->SetCurrentUserActive( UserIndex, LiveSession, true ); // @todo: remove user index param

				auto CurrentHost = FOnlineSessionLive::GetLiveSessionHost(LiveSession);

				// Greedy host selection - if there isn't a host yet, try to become the host.
				// Uses the synchronous update so that Live will make sure only one console succeeds
				// in becoming the host.
				if ( !CurrentHost )
				{
					LiveSession = SessionInterface->SetHostDeviceTokenSynchronous(UserIndex, SessionName, LiveSession, LiveContext);

					if (auto NamedSession = SessionInterface->GetNamedSession(SessionName))
					{
						NamedSession->HostingPlayerNum = UserIndex;
						NamedSession->bHosting = true;
						if (LiveSession->CurrentUser)
						{
							NamedSession->LocalOwnerId = MakeShared<FUniqueNetIdLive>(LiveSession->CurrentUser->XboxUserId);
						}
					}

					if(!LiveSession)
					{
						UE_LOG_ONLINE(Log, TEXT("Failed to set host device token."));
						bWasSuccessful = false;
						bIsComplete = true;
						return;
					}
				}

				MultiplayerSessionMember^ NewHostMember = FOnlineSessionLive::GetLiveSessionHost(LiveSession);
				for(auto Member : LiveSession->Members)
				{
					if(Member->IsCurrentUser && ( Member->DeviceToken == LiveSession->SessionProperties->HostDeviceToken ) )
					{
						// If this console is the host, it will wait for SecureDeviceAssociatons from the clients,
						// so we're done here.
						UE_LOG_ONLINE(Log, TEXT("This console is the session host."));

						bWasSuccessful = (LiveSession != nullptr);
						bIsComplete = true;
						return;
					}
				}

				// This console is a client, establish a SecureDeviceAssociation with the host.
				auto HostSDABase64 = NewHostMember->SecureDeviceAddressBase64;

				try
				{
					auto SDA = SecureDeviceAddress::FromBase64String(HostSDABase64);

					auto SDATemplate = Subsystem->GetSessionInterfaceLive()->GetSDATemplate();
					if(!SDATemplate)
					{
						UE_LOG_ONLINE(Log, TEXT("Invalid secure device association template."));
						bWasSuccessful = false;
						bIsComplete = true;
						return;
					}

					auto asyncOp = SDATemplate->CreateAssociationAsync(SDA, CreateSecureDeviceAssociationBehavior::Default);
					// @ATG_CHANGE : BEGIN - UWP LIVE support
					create_task(asyncOp).then([this](task<SecureDeviceAssociation^> InAssociationTask)
					{
						try
						{
							Association = InAssociationTask.get();
							UE_LOG_ONLINE(Log, TEXT("Created association in matchmaking, now in state %s"),
								Association->State.ToString()->Data());

							auto StateChangedEvent = ref new TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>(&FOnlineSessionLive::LogAssociationStateChange);
							Association->StateChanged += StateChangedEvent;

							// Set activity to new session. This will be the session used for invites/join in progress if supported.
							Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveSetSessionActivity>(Subsystem, LiveContext, LiveSession->SessionReference);

							// Store the host address, will be set in Finalize() since Finalize() runs on the game thread.
							HostAddr = FOnlineSessionLive::GetAddrFromDeviceAssociation(Association);

							bWasSuccessful = true;
							bIsComplete = true;
						}
						catch(Platform::Exception^ Ex)
						{
							UE_LOG_ONLINE(Warning, TEXT("Invalid host secure device address."));

							bWasSuccessful = false;
							bIsComplete = true;
						}
					});
					// @ATG_CHANGE : END
				}
				catch(Platform::Exception^ Ex)
				{
					UE_LOG_ONLINE(Warning, TEXT("Invalid host secure device address."));

					bWasSuccessful = false;
					bIsComplete = true;
				}
			}
			else
			{
				bWasSuccessful = false;
				bIsComplete = true;
			}
		}
		catch (Platform::Exception^ ex)
		{
			UE_LOG(LogOnline, Log, L"OnGameSessionReady: GetCurrentSessionAsync failed with 0x%0.8X", ex->HResult);
			bIsComplete = true;
			bWasSuccessful = false;
		}
	// @ATG_CHANGE : BEGIN -Synchronous wait in Epic code requires different continuation semantics for UWP
	}, task_continuation_context::use_arbitrary());
	// @ATG_CHANGE : END

}

void FOnlineAsyncTaskLiveGameSessionReady::Finalize()
{
	if (!bWasSuccessful)
	{
		// @v2live Should this use CreateDestroyTask to ensure session is actually cleared out?
		Subsystem->GetSessionInterfaceLive()->RemoveNamedSession(SessionName);
		return;
	}

	// Get the Live session info
	auto NamedSession = Subsystem->GetSessionInterfaceLive()->GetNamedSession(SessionName);
	check(NamedSession != nullptr);

	FOnlineSessionLive::ReadSettingsFromLiveJson( LiveSession, *NamedSession );
	NamedSession->SessionState = EOnlineSessionState::Pending;

	FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
	check(LiveInfo.IsValid());

	Subsystem->RefreshLiveInfo(SessionName, LiveSession);
	LiveInfo->SetHostAddr(HostAddr);
	LiveInfo->SetAssociation(Association);

	Subsystem->GetSessionInterfaceLive()->DetermineSessionHost(SessionName, LiveSession);

	// Host will re-advertise the match
	FOnlineMatchmakingInterfaceLivePtr MatchmakingInterface = Subsystem->GetMatchmakingInterfaceLive();
	MatchmakingInterface->SetTicketState(SessionName, EOnlineLiveMatchmakingState::Active);
	LiveInfo->SetSessionReady();
	Subsystem->GetMatchmakingInterfaceLive()->SubmitMatchingTicket(LiveSession->SessionReference, SessionName, false);
}

void FOnlineAsyncTaskLiveGameSessionReady::TriggerDelegates()
{
	Subsystem->GetMatchmakingInterfaceLive()->TriggerOnMatchmakingCompleteDelegates(SessionName, bWasSuccessful);
}
