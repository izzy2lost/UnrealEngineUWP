// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveJoinSession.h"
#include "OnlineAsyncTaskLiveRegisterLocalUser.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "OnlineAsyncTaskLiveRegisterLocalUser.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "../OnlineMatchmakingInterfaceLive.h"
#include "OnlineAsyncTaskLiveSetSessionActivity.h"

#include "OnlineAsyncTaskLiveJoinSession.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace concurrency;

using Windows::Xbox::System::User;
using Microsoft::Xbox::Services::XboxLiveContext;
using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember;

FOnlineAsyncTaskLiveJoinSession::FOnlineAsyncTaskLiveJoinSession(
	FOnlineSessionLive* InLiveInterface,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InReference,
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ InTemplate,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FNamedOnlineSession* InNamedSession,
	class FOnlineSubsystemLive* Subsystem,
	int RetryCount,
	bool bSessionIsMatchmakingResult,
	const bool bInSetActivity,
	const TOptional<FString>& InSessionInviteHandle)
	: FOnlineAsyncTaskBasic(Subsystem)
	, SessionInterface(InLiveInterface)
	, SessionReference(InReference)
	, PeerTemplate(InTemplate)
	, NamedSession(InNamedSession)
	, LiveContext(InContext)
	, Association(nullptr)
	, LiveSession(nullptr)
	, JoinResult(EOnJoinSessionCompleteResult::Success)
	, RetryCount(RetryCount)
	, bIsMatchmakingResult(bSessionIsMatchmakingResult)
	, OtherLocalPlayersToAdd(0)
	, bSetActivity(bInSetActivity)
	, SessionName(NamedSession->SessionName)
	, SessionInviteHandle(InSessionInviteHandle)
{
	Retry(true);
}

void FOnlineAsyncTaskLiveJoinSession::OnSuccess()
{
	// This will be the session used for invites/join in progress if supported.
	if (bSetActivity)
	{
		Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveSetSessionActivity>(Subsystem, LiveContext, LiveSession->SessionReference);
	}

	JoinResult = EOnJoinSessionCompleteResult::Success;
	bWasSuccessful = true;
	bIsComplete = true;
}

void FOnlineAsyncTaskLiveJoinSession::OnFailed(EOnJoinSessionCompleteResult::Type Result)
{
	JoinResult = Result;
	bWasSuccessful = false;
	bIsComplete = true;
}

void FOnlineAsyncTaskLiveJoinSession::Retry(bool bGetSession)
{
	if ( RetryCount <= 0 )
	{
		OnFailed(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	--RetryCount;

	if (bGetSession)
	{
		IAsyncOperation<MultiplayerSession^>^ SessionFetchTask = nullptr;
		if (SessionInviteHandle.IsSet())
		{
			SessionFetchTask = LiveContext->MultiplayerService->GetCurrentSessionByHandleAsync(ref new Platform::String(*SessionInviteHandle.GetValue()));
		}
		else
		{
			SessionFetchTask = LiveContext->MultiplayerService->GetCurrentSessionAsync(SessionReference);
		}
		
		create_task(SessionFetchTask).then([this](task<MultiplayerSession^> SessionTask)
		{
			try
			{
				LiveSession = SessionTask.get();

				TryJoinSession();
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
					OnFailed(EOnJoinSessionCompleteResult::SessionDoesNotExist);
				}
			}
		});
	}
	else
	{
		TryJoinSession();
	}
}

void FOnlineAsyncTaskLiveJoinSession::TryJoinSession()
{
	// Session may be null if it timed out after the SessionReference was stored
	if (LiveSession == nullptr)
	{
		OnFailed(EOnJoinSessionCompleteResult::SessionDoesNotExist);
		return;
	}

	// Don't join if the session is full.
	// @ATG_CHANGE : UWP Live Support - BEGIN
	if (!FOnlineSessionLive::CanUserJoinSession(SystemUserFromXSAPIUser(LiveContext->User), LiveSession))
	// @ATG_CHANGE : UWP Live Support - END
	{
		OnFailed(EOnJoinSessionCompleteResult::SessionIsFull);
		return;
	}

	// For matchmaking, identify local users for proper join and QoS handling
	Platform::Collections::Vector<MultiplayerSessionMember^>^ LocalMembers =
		ref new Platform::Collections::Vector<MultiplayerSessionMember^>();

	if (bIsMatchmakingResult)
	{
		FOnlineIdentityLivePtr IdentityInt(Subsystem->GetIdentityLive());
		check(IdentityInt.IsValid());

		{
			IVectorView<User^>^ CachedUsers = IdentityInt->GetCachedUsers();
			for (MultiplayerSessionMember^ Member : LiveSession->Members)
			{
				for (User^ User : CachedUsers)
				{
					if (Member->XboxUserId == User->XboxUserId)
					{
						// Found a local member
						LocalMembers->Append(Member);
						break;
					}
				}
			}
		}

		// Ensure all local members pass or fail QoS together by putting them in an initialization group.
		// Other local members will also need to set this in RegisterLocalUserAsync.
		LiveSession->SetCurrentUserMembersInGroup(LocalMembers->GetView());

		// Don't need to call join, matchmaking did that automatically
	}
	else
	{
		LiveSession->Join();
	}

	LiveSession->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
	LiveSession->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());

	// Indicate what events to subscribe to. Also handle joining matchmaking target session prior to QoS.
	// Also use TryWriteSessionAsync when committing changes

	LiveSession->SetSessionChangeSubscription(MultiplayerSessionChangeTypes::Everything);

	if (bIsMatchmakingResult && NamedSession->SessionInfo.IsValid())
	{
		// In matchmaking, the session info already exists, so just update it.
		// @v2live Should this branch off bIsMatchmakingResult, or should we just check if SessionInfo exists?
		FOnlineSessionInfoLivePtr LiveInfo(StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo));
		LiveInfo->RefreshLiveInfo(LiveSession);
	}
	else
	{
		NamedSession->SessionInfo = MakeShared<FOnlineSessionInfoLive>(LiveSession);
	}

	NamedSession->SessionState = EOnlineSessionState::Pending;

	IAsyncOperation<WriteSessionResult^>^ JoinOp = nullptr;
	try
	{
		if (SessionInviteHandle.IsSet())
		{
			JoinOp = LiveContext->MultiplayerService->TryWriteSessionByHandleAsync(LiveSession, MultiplayerSessionWriteMode::SynchronizedUpdate, ref new Platform::String(*SessionInviteHandle.GetValue()));
		}
		else
		{
			JoinOp = LiveContext->MultiplayerService->TryWriteSessionAsync(LiveSession, MultiplayerSessionWriteMode::SynchronizedUpdate);
		}
	}
	catch (Platform::Exception^ Ex)
	{
		
		OnFailed(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	create_task(JoinOp).then([this, LocalMembers](task<WriteSessionResult^> Task)
	{
		try
		{
			WriteSessionResult^ Result = Task.get();
			LiveSession = Result->Session;

			if (Result->Succeeded)
			{
				// Register for shouldertaps (push-notifications from xbox about session changes)
				Subsystem->GetSessionMessageRouter()->AddOnSessionChangedDelegate(Subsystem->GetSessionInterfaceLive()->OnSessionChangedDelegate, LiveSession->SessionReference);

				if (bIsMatchmakingResult)
				{
					TryJoinSessionFromMatchmaking(LocalMembers);
				}
				else if (NamedSession->SessionSettings.bIsDedicated)
				{
					TryJoinSessionFromDedicated();
				}
				else if (MultiplayerSessionMember^ Host = FOnlineSessionLive::GetLiveSessionHost(LiveSession))
				{
					TryJoinSessionFromPeer(Host);
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("Failed to join session: no host player and session not dedicated."));
					OnFailed(EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
					return;
				}
			}	// if (Result->Succeeded)
			else if (Result->Status == WriteSessionStatus::OutOfSync)
			{
				Retry(false);
			}
			else
			{
				OnFailed(EOnJoinSessionCompleteResult::UnknownError);
			}
		}
		catch(Platform::Exception^ Ex)
		{
			OnFailed(EOnJoinSessionCompleteResult::UnknownError);
		}
	});
}

void FOnlineAsyncTaskLiveJoinSession::TryJoinSessionFromMatchmaking(Platform::Collections::Vector<MultiplayerSessionMember ^>^ LocalMembers)
{
	// There may have been other local users in the matchmaking session. Put them
	// in the game session as well.

	OtherLocalPlayersToAdd = 0;
	bWasSuccessful = true;	// assume success unless a RegisterLocalPlayer delegate changes this

	Subsystem->RefreshLiveInfo(SessionName, LiveSession);

	for (MultiplayerSessionMember^ Member : LocalMembers)
	{
		if (Member->XboxUserId != LiveSession->CurrentUser->XboxUserId)
		{
			OtherLocalPlayersToAdd++;

			FUniqueNetIdLive MemberId(Member->XboxUserId);
			XboxLiveContext^ MemberContext = Subsystem->GetLiveContext(MemberId);

			Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveRegisterLocalUser>(
				SessionName,
				MemberContext,
				Subsystem,
				MoveTemp(MemberId),
				FOnRegisterLocalPlayerCompleteDelegate::CreateRaw(this, &FOnlineAsyncTaskLiveJoinSession::OnAddLocalPlayerComplete),
				LocalMembers->GetView());
		}
	}

	bIsComplete = (OtherLocalPlayersToAdd == 0);

	// Connecting to the host, etc., will happen in the GameSessionReady task after
	// QoS is complete.
}

void FOnlineAsyncTaskLiveJoinSession::TryJoinSessionFromDedicated()
{
	// Success, we're in the session, but it's still up to the game to get onto the server
	OnSuccess();
}

void FOnlineAsyncTaskLiveJoinSession::TryJoinSessionFromPeer(MultiplayerSessionMember^ Host)
{
	if (!PeerTemplate)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to join session: no PeerTemplate"));
		OnFailed(EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
		return;
	}

	try
	{
		auto HostSDABase64 = Host->SecureDeviceAddressBase64;
		auto SDA = SecureDeviceAddress::FromBase64String(HostSDABase64);

		IAsyncOperation<SecureDeviceAssociation^ >^ SDAOp = PeerTemplate->CreateAssociationAsync(SDA, CreateSecureDeviceAssociationBehavior::Default);
		create_task(SDAOp).then([this](task<SecureDeviceAssociation^> AssociationTask)
		{
			try
			{
				Association = AssociationTask.get();

				UE_LOG_ONLINE(Log, TEXT("Created association, now in state %s"),
					FOnlineSessionLive::AssociationStateToString(Association->State));

				auto StateChangedEvent = ref new TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>(&FOnlineSessionLive::LogAssociationStateChange);
				Association->StateChanged += StateChangedEvent;

				OnSuccess();
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("Failed to join session: failed to create secure device association with host."));
				OnFailed(EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
			}
		});
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to join session: has host with invalid secure device address."));
		OnFailed(EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
		return;
	}
}

void FOnlineAsyncTaskLiveJoinSession::OnAddLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result)
{
	FPlatformAtomics::InterlockedDecrement(&OtherLocalPlayersToAdd);

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskLiveJoinSession::OnAddLocalPlayerComplete: failed to add local player to game session with result %u"), Result);
		bWasSuccessful = false;
	}

	if (OtherLocalPlayersToAdd <= 0)
	{
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskLiveJoinSession::Finalize()
{
	{
		const bool WasSuccessful = bWasSuccessful;
		UE_LOG_ONLINE(Verbose, TEXT("JoinSessionLive Complete bWasSuccessful: %d SessionId: %ls"), WasSuccessful, SessionReference->ToUriPath()->Data());
	}
	if (!bWasSuccessful && NamedSession != nullptr)
	{
		// Clean up partial create/join
		SessionInterface->RemoveNamedSession(SessionName);
		return;
	}

	if (NamedSession && NamedSession->SessionInfo.IsValid() && !bIsMatchmakingResult)
	{
		if (Association)
		{
			FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(NamedSession->SessionInfo);
			TSharedRef<FInternetAddr> Addr = FOnlineSessionLive::GetAddrFromDeviceAssociation(Association);
			LiveInfo->SetHostAddr(Addr);
			LiveInfo->SetAssociation(Association);
		}

		// Update with the new session since we wrote to it
		// For the matchmaking case, this has already been done
		Subsystem->RefreshLiveInfo(SessionName, LiveSession);
	}

	// Initialize session state after create/join
	if (bWasSuccessful)
	{
		Subsystem->GetSessionMessageRouter()->SyncInitialSessionState(SessionName, LiveSession);
	}
}

void FOnlineAsyncTaskLiveJoinSession::TriggerDelegates()
{
	if (!bWasSuccessful && bIsMatchmakingResult)
	{
		// This join was part of session initialization during matchmaking, and it failed. Matchmaking
		// needs to fail as well.
		Subsystem->GetMatchmakingInterfaceLive()->TriggerOnMatchmakingCompleteDelegates(SessionName, bWasSuccessful);
	}

	// In matchmaking, this isn't a final state, and we don't want to trigger any unrelated delegates.
	if (!bIsMatchmakingResult)
	{
		SessionInterface->TriggerOnJoinSessionCompleteDelegates(SessionName, JoinResult);
	}
}
