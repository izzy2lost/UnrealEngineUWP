// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationManager.h"

#include "ConcertLogGlobal.h"
#include "IConcertSyncClient.h"
#include "Replication/ChangeOperationTypes.h"
#include "Replication/IConcertClientReplicationManager.h"

#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FMultiUserReplicationManager"

namespace UE::MultiUserClient::Replication
{
	FMultiUserReplicationManager::FMultiUserReplicationManager(TSharedRef<IConcertSyncClient> InClient)
		: Client(MoveTemp(InClient))
	{
		Client->GetConcertClient()->OnSessionConnectionChanged().AddRaw(
			this,
			&FMultiUserReplicationManager::OnSessionConnectionChanged
			);
	}

	FMultiUserReplicationManager::~FMultiUserReplicationManager()
	{
		Client->GetConcertClient()->OnSessionConnectionChanged().RemoveAll(this);
	}

	void FMultiUserReplicationManager::JoinReplicationSession()
	{
		IConcertClientReplicationManager* Manager = Client->GetReplicationManager();
		if (!ensure(ConnectionState == EMultiUserReplicationConnectionState::Disconnected)
			|| !ensure(Manager))
		{
			return;
		}

		ConnectionState = EMultiUserReplicationConnectionState::Connecting;
		Manager->JoinReplicationSession({})
			.Next([WeakThis = AsWeak()](ConcertSyncClient::Replication::FJoinReplicatedSessionResult&& JoinSessionResult)
			{
				// The future can execute on any thread
				ExecuteOnGameThread(TEXT("JoinReplicationSession"), [WeakThis, JoinSessionResult = MoveTemp(JoinSessionResult)]()
				{
					// Shutting down engine?
					if (const TSharedPtr<FMultiUserReplicationManager> ThisPin = WeakThis.Pin())
					{
						ThisPin->HandleReplicationSessionJoined(JoinSessionResult);
					}
				});
			});
	}

	void FMultiUserReplicationManager::OnSessionConnectionChanged(
		IConcertClientSession& ConcertClientSession,
		EConcertConnectionStatus ConcertConnectionStatus
		)
	{
		switch (ConcertConnectionStatus)
		{
		case EConcertConnectionStatus::Connecting:
			break;
		case EConcertConnectionStatus::Connected:
			JoinReplicationSession();
			break;
		case EConcertConnectionStatus::Disconnecting:
			break;
		case EConcertConnectionStatus::Disconnected:
			OnLeaveSession(ConcertClientSession);
			break;
		default: ;
		}
	}

	void FMultiUserReplicationManager::OnLeaveSession(IConcertClientSession&)
	{
		// This destroys the UI and tells any other potential system to stop referencing anything in ConnectedState (such as shared ptrs)...
		SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState::Disconnected);
		// ... so now it is safe to destroy ConnectedState.
		ConnectedState.Reset();
	}

	void FMultiUserReplicationManager::HandleReplicationSessionJoined(const ConcertSyncClient::Replication::FJoinReplicatedSessionResult& JoinSessionResult)
	{
		const bool bSuccess = JoinSessionResult.ErrorCode == EJoinReplicationErrorCode::Success;
		if (bSuccess)
		{
			ConnectedState.Emplace(Client, DiscoveryContainer);
			SetupClientConnectionEvents();
			SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState::Connected);

			// For convenience, the client should attempt to restore the content when they last left.
			RestoreContentFromLastTime();
		}
		else
		{
			SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState::Disconnected);
		}
	}

	void FMultiUserReplicationManager::SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState NewState)
	{
		ConnectionState = NewState;
		OnReplicationConnectionStateChangedDelegate.Broadcast(ConnectionState);
	}

	void FMultiUserReplicationManager::RestoreContentFromLastTime()
	{
		Client->GetReplicationManager()->RestoreContent(
			{
				.Flags = EConcertReplicationRestoreContentFlags::All | EConcertReplicationRestoreContentFlags::ValidateUniqueClient
			}
		)
		.Next([ClientInfo = Client->GetConcertClient()->GetClientInfo()](FConcertReplication_RestoreContent_Response&& Response)
		{
			UE_LOG(LogConcert, Log, TEXT("Content restoration completed with result '%s'"), *ConcertSyncCore::LexToString(Response.ErrorCode));
			const bool bIsTimeout = !IsInGameThread() || Response.ErrorCode == EConcertReplicationRestoreErrorCode::Timeout;
			if (Response.IsSuccess() || bIsTimeout || !FSlateApplication::IsInitialized())
			{
				return;
			}

			FSlateNotificationManager& NotificationManager = FSlateNotificationManager::Get();
			FNotificationInfo NotificationInfo(LOCTEXT("RestoreFailed.Main", "Replication Content Restore"));
			NotificationInfo.SubText = FText::Format(
				LOCTEXT("RestoreFailed.SubTextFmt", "Display name {0} and device name {1} already taken by another client in session."),
				FText::FromString(ClientInfo.DisplayName),
				FText::FromString(ClientInfo.DeviceName)
				);
			NotificationInfo.bFireAndForget = true;
			NotificationInfo.bUseSuccessFailIcons = true;
			NotificationInfo.ExpireDuration = 4.f;
			NotificationManager.AddNotification(NotificationInfo)
				->SetCompletionState(SNotificationItem::CS_Fail);
		});
	}

	void FMultiUserReplicationManager::SetupClientConnectionEvents()
	{
		FOnlineClientManager& ClientManager = ConnectedState->ClientManager;
		ClientManager.ForEachClient([this](FOnlineClient& InClient){ SetupClientDelegates(InClient); return EBreakBehavior::Continue; });
		ClientManager.OnPostRemoteClientAdded().AddRaw(this, &FMultiUserReplicationManager::OnReplicationClientConnected);
	}

	void FMultiUserReplicationManager::OnClientStreamServerStateChanged(const FGuid EndpointId) const
	{
		UE_LOG(LogConcert, Verbose, TEXT("Client %s stream changed"), *EndpointId.ToString());
		OnStreamServerStateChangedDelegate.Broadcast(EndpointId);
	}

	void FMultiUserReplicationManager::OnClientAuthorityServerStateChanged(const FGuid EndpointId) const
	{
		UE_LOG(LogConcert, Verbose, TEXT("Client %s authority changed"), *EndpointId.ToString());
		OnAuthorityServerStateChangedDelegate.Broadcast(EndpointId);
	}

	void FMultiUserReplicationManager::SetupClientDelegates(FOnlineClient& InClient) const
	{
		InClient.GetStreamSynchronizer().OnServerStreamChanged().AddRaw(this, &FMultiUserReplicationManager::OnClientStreamServerStateChanged, InClient.GetEndpointId());
		InClient.GetAuthoritySynchronizer().OnServerAuthorityChanged().AddRaw(this, &FMultiUserReplicationManager::OnClientAuthorityServerStateChanged, InClient.GetEndpointId());
	}

	const FConcertObjectReplicationMap* FMultiUserReplicationManager::FindReplicationMapForClient(const FGuid& ClientId) const
	{
		if (ConnectedState && ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			const FOnlineClient* ReplicationClient = ConnectedState->ClientManager.FindClient(ClientId);
			return ReplicationClient
				? &ReplicationClient->GetStreamSynchronizer().GetServerState()
				: nullptr;
		}
		return nullptr;
	}

	const FConcertStreamFrequencySettings* FMultiUserReplicationManager::FindReplicationFrequenciesForClient(const FGuid& ClientId) const
	{
		if (ConnectedState && ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			const FOnlineClient* ReplicationClient = ConnectedState->ClientManager.FindClient(ClientId);
			return ReplicationClient
				? &ReplicationClient->GetStreamSynchronizer().GetFrequencySettings()
				: nullptr;
		}
		return nullptr;
	}

	bool FMultiUserReplicationManager::IsReplicatingObject(const FGuid& ClientId, const FSoftObjectPath& ObjectPath) const 
	{
		if (ConnectedState && ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			const FOnlineClient* ReplicationClient = ConnectedState->ClientManager.FindClient(ClientId);
			return ReplicationClient && ReplicationClient->GetAuthoritySynchronizer().HasAuthorityOver(ObjectPath);
		}
		return false;
	}

	void FMultiUserReplicationManager::RegisterReplicationDiscoverer(TSharedRef<IReplicationDiscoverer> Discoverer)
	{
		if (ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			DiscoveryContainer.AddDiscoverer(Discoverer);
		}
	}

	void FMultiUserReplicationManager::RemoveReplicationDiscoverer(const TSharedRef<IReplicationDiscoverer>& Discoverer)
	{
		if (ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			DiscoveryContainer.RemoveDiscoverer(Discoverer);
		}
	}

	TSharedRef<IClientChangeOperation> FMultiUserReplicationManager::EnqueueChanges(const FGuid& ClientId, TAttribute<FChangeClientReplicationRequest> SubmissionParams)
	{
		if (!ensureMsgf(IsInGameThread(), TEXT("To simplify implementation, only calls from game thread are allowed.")))
		{
			return FExternalClientChangeRequestHandler::MakeFailedOperation(EChangeStreamOperationResult::NotOnGameThread, EChangeAuthorityOperationResult::NotOnGameThread);
		}
		
		if (ConnectedState)
		{
			FOnlineClient* ReplicationClient = ConnectedState->ClientManager.FindClient(ClientId);
			return ReplicationClient
				? ReplicationClient->GetExternalRequestHandler().HandleRequest(MoveTemp(SubmissionParams))
				: FExternalClientChangeRequestHandler::MakeFailedOperation(EChangeStreamOperationResult::UnknownClient, EChangeAuthorityOperationResult::UnknownClient);
		}
		return FExternalClientChangeRequestHandler::MakeFailedOperation(EChangeStreamOperationResult::NotInSession, EChangeAuthorityOperationResult::NotInSession);
	}

	FMultiUserReplicationManager::FConnectedState::FConnectedState(TSharedRef<IConcertSyncClient> InClient, FReplicationDiscoveryContainer& InDiscoveryContainer)
		: Client(InClient)
		, QueryService(*InClient)
		, ClientManager(InClient, InClient->GetConcertClient()->GetCurrentSession().ToSharedRef(), InDiscoveryContainer, QueryService.GetStreamAndAuthorityQueryService())
		, MuteManager(*InClient, QueryService.GetMuteStateQueryService(), ClientManager.GetAuthorityCache())
		, PresetManager(*InClient, ClientManager, MuteManager.GetSynchronizer())
		, PropertySelector(ClientManager)
		, ChangeLevelHandler(ClientManager.GetLocalClient().GetClientEditModel().Get())
		, PreventReplicatedPropertyTransaction(*InClient, ClientManager, MuteManager)
		, UserNotifier(ClientManager, MuteManager)
	{}
}

#undef LOCTEXT_NAMESPACE