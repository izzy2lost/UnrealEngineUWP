// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

#include "OnlineAsyncTaskLiveJoinSession.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace concurrency;


FOnlineAsyncTaskLiveJoinSession::FOnlineAsyncTaskLiveJoinSession(
	FOnlineSessionLive* InLiveInterface,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InReference,
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ InTemplate,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FNamedOnlineSession* InNamedSession,
	class FOnlineSubsystemLive* Subsystem,
	int RetryCount)
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
	, bIsMatchmakingResult(false)
	, OtherLocalPlayersToAdd(0)
{
	Retry(true);
}

FOnlineAsyncTaskLiveJoinSession::FOnlineAsyncTaskLiveJoinSession(
	FOnlineSessionLive* InLiveInterface,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InReference,
	Windows::Xbox::Networking::SecureDeviceAssociationTemplate^ InTemplate,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FNamedOnlineSession* InNamedSession,
	class FOnlineSubsystemLive* Subsystem,
	int RetryCount,
	bool bSessionIsMatchmakingResult)
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
{
	Retry(true);
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
		auto GetSessionOp = LiveContext->MultiplayerService->GetCurrentSessionAsync(SessionReference);
		create_task(GetSessionOp).then([this](task<MultiplayerSession^> SessionTask)
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
					UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Failed to get session from session reference."));
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
	if (!Subsystem->GetSessionInterfaceLive()->CanUserJoinSession(SystemUserFromXSAPIUser(LiveContext->User), LiveSession))
	{
		OnFailed(EOnJoinSessionCompleteResult::SessionIsFull);
		return;
	}
	
	// For matchmaking, identify local users for proper join and QoS handling
	Platform::Collections::Vector<MultiplayerSessionMember^>^ LocalMembers =
		ref new Platform::Collections::Vector<MultiplayerSessionMember^>();

	if (bIsMatchmakingResult)
	{
		for (auto Member : LiveSession->Members)
		{
			// @ATG_CHANGE : BEGIN UWP LIVE support
			if (LiveSession->CurrentUser == nullptr || Member->XboxUserId != LiveSession->CurrentUser->XboxUserId)
			{
				FUniqueNetIdLive MemberId(Member->XboxUserId);
				auto MemberUser = Subsystem->GetIdentityLive()->GetUserForUniqueNetId(MemberId);

				if (MemberUser != nullptr && MemberUser)
				{
					// Found a local member
					LocalMembers->Append(Member);
				}
			}
			// @ATG_CHANGE : END
		}

		// Ensure all local members pass or fail QoS together by putting them in an initialization group.
		// Other local members will also need to set this in RegisterLocalUserAsync.
		LiveSession->SetCurrentUserMembersInGroup(LocalMembers->GetView());

		// Don't need to call join, matchmaking did that automatically
	}
	else
	{
		LiveSession->Join(nullptr);
	}

	LiveSession->SetCurrentUserStatus( MultiplayerSessionMemberStatus::Active );
	LiveSession->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());
	
	// Indicate what events to subscribe to. Also handle joining matchmaking target session prior to QoS.
	// Also use TryWriteSessionAsync when committing changes

	LiveSession->SetSessionChangeSubscription(MultiplayerSessionChangeTypes::Everything);
						
	FOnlineSessionInfoLive* LiveInfo;

	if(bIsMatchmakingResult && NamedSession->SessionInfo.IsValid())
	{
		// In matchmaking, the session info already exists, so just update it.
		// @v2live Should this branch off bIsMatchmakingResult, or should we just check if SessionInfo exists?
		LiveInfo = static_cast<FOnlineSessionInfoLive*>(NamedSession->SessionInfo.Get());
		LiveInfo->RefreshLiveInfo(LiveSession);
	}
	else
	{
		LiveInfo = new FOnlineSessionInfoLive(LiveSession);
		NamedSession->SessionInfo = MakeShareable(LiveInfo);
	}

	NamedSession->SessionState = EOnlineSessionState::Pending;

	auto JoinOp = LiveContext->MultiplayerService->TryWriteSessionAsync(LiveSession, MultiplayerSessionWriteMode::SynchronizedUpdate);
	create_task(JoinOp).then([this, LiveInfo, LocalMembers](task<WriteSessionResult^> Task)
	{
		try
		{
			WriteSessionResult^ Result = Task.get();
			LiveSession = Result->Session;

			if (Result->Succeeded)
			{
				//Register for shouldertaps.
				Subsystem->GetSessionMessageRouter()->AddOnSessionChangedDelegate(Subsystem->GetSessionInterfaceLive()->OnSessionChangedDelegate, LiveSession->SessionReference);

				if (bIsMatchmakingResult)
				{
					// There may have been other local users in the matchmaking session. Put them
					// in the game session as well.

					OtherLocalPlayersToAdd = 0;
					bWasSuccessful = true;	// assume success unless a RegisterLocalPlayer delegate changes this

					Subsystem->RefreshLiveInfo(NamedSession->SessionName, LiveSession);
						
					for (auto Member : LocalMembers)
					{
						if (Member->XboxUserId != LiveSession->CurrentUser->XboxUserId)
						{
							OtherLocalPlayersToAdd++;

							FUniqueNetIdLive MemberId(Member->XboxUserId);
							auto MemberContext = Subsystem->GetLiveContext(MemberId);

							FOnlineAsyncTaskLiveRegisterLocalUser* RegisterTask = new FOnlineAsyncTaskLiveRegisterLocalUser(
								NamedSession->SessionName,
								MemberContext,
								Subsystem,
								MemberId,
								FOnRegisterLocalPlayerCompleteDelegate::CreateRaw(this, &FOnlineAsyncTaskLiveJoinSession::OnAddLocalPlayerComplete),
								LocalMembers->GetView());
							Subsystem->GetAsyncTaskManager()->AddToParallelTasks(RegisterTask);
						}
					}

					bIsComplete = (OtherLocalPlayersToAdd == 0);		

					// Connecting to the host, etc., will happen in the GameSessionReady task after
					// QoS is complete.
				}
				else	// !bIsMatchmakingResult
				{
					// Get the host token.
					MultiplayerSessionMember^ Host = FOnlineSessionLive::GetLiveSessionHost(LiveSession);

					auto HostSDABase64 = Host->SecureDeviceAddressBase64;

					try
					{
						auto SDA = SecureDeviceAddress::FromBase64String(HostSDABase64);
						IAsyncOperation<SecureDeviceAssociation^ >^ SDAOp = PeerTemplate->CreateAssociationAsync(SDA, CreateSecureDeviceAssociationBehavior::Default);
						create_task(SDAOp).then([this](task<SecureDeviceAssociation^> AssociationTask)
						{
							try
							{
								Association = AssociationTask.get();

								UE_LOG(LogOnlineSubsystemLive, Log, TEXT("Created association, now in state %s"),
								FOnlineSessionLive::AssociationStateToString(Association->State));

								auto StateChangedEvent = ref new TypedEventHandler<SecureDeviceAssociation^, SecureDeviceAssociationStateChangedEventArgs^>(&FOnlineSessionLive::LogAssociationStateChange);
								Association->StateChanged += StateChangedEvent;
								
								// This will be the session used for invites/join in progress if supported.
								LiveContext->MultiplayerService->SetActivityAsync(LiveSession->SessionReference);

								JoinResult = EOnJoinSessionCompleteResult::Success;
								bWasSuccessful = true;
								bIsComplete = true;
							}
							catch(Platform::Exception^ Ex)
							{
								UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Failed to create secure device associtaion with host."));
								OnFailed(EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
							}
						});
					}
					catch(Platform::Exception^ Ex)
					{
						UE_LOG(LogOnlineSubsystemLive, Warning, TEXT("Invalid host secure device address."));
						OnFailed(EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
						return;
					}
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

void FOnlineAsyncTaskLiveJoinSession::OnAddLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result)
{
	FPlatformAtomics::InterlockedDecrement(&OtherLocalPlayersToAdd);

	if(Result != EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(LogOnline, Error, TEXT("FOnlineAsyncTaskLiveJoinSession::OnAddLocalPlayerComplete: failed to add local player to game session with result %u"), static_cast<uint32>(Result));
		bWasSuccessful = false;
	}

	if(OtherLocalPlayersToAdd <= 0)
	{
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskLiveJoinSession::Finalize()
{
	if (!WasSuccessful() && NamedSession != nullptr)
	{
		// Clean up partial create/join
		SessionInterface->RemoveNamedSession(NamedSession->SessionName);
		return;
	}

	if(NamedSession && NamedSession->SessionInfo.IsValid() && !bIsMatchmakingResult)
	{
		if (Association)
		{
			auto LiveInfo = static_cast<FOnlineSessionInfoLive*>(NamedSession->SessionInfo.Get());
			auto Addr = FOnlineSessionLive::GetAddrFromDeviceAssociation(Association);
			LiveInfo->SetHostAddr(Addr);
			LiveInfo->SetAssociation(Association);
		}
			
		// Update with the new session since we wrote to it
		// For the matchmaking case, this has already been done
		Subsystem->RefreshLiveInfo(NamedSession->SessionName, LiveSession);
	}

	// Initialize session state after create/join
	if(WasSuccessful())
	{
		Subsystem->GetSessionMessageRouter()->SyncInitialSessionState(NamedSession->SessionName, LiveSession);
	}
}

void FOnlineAsyncTaskLiveJoinSession::TriggerDelegates()
{
	if (!WasSuccessful() && bIsMatchmakingResult)
	{
		// This join was part of session initialization during matchmaking, and it failed. Matchmaking
		// needs to fail as well.
		Subsystem->GetMatchmakingInterfaceLive()->TriggerOnMatchmakingCompleteDelegates(NamedSession->SessionName, bWasSuccessful);
	}

	// In matchmaking, this isn't a final state, and we don't want to trigger any unrelated delegates.
	if (!bIsMatchmakingResult)
	{
		SessionInterface->TriggerOnJoinSessionCompleteDelegates(NamedSession->SessionName, JoinResult);
	}
}
