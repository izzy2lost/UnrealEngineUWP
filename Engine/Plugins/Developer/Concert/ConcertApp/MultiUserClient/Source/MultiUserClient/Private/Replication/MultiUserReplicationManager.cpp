// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationManager.h"

#include "IConcertSyncClient.h"
#include "Replication/IConcertClientReplicationManager.h"

#include "Containers/Ticker.h"
#include "UObject/Package.h"

namespace UE::MultiUserClient
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
		// For now we join without any initial data - this will likely change in the future (5.5+)
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
		// This clears the UI. The clients' IEditableReplicationStreamModels should no longer be referenced by anyone.
		SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState::Disconnected);
		// Keep in mind the IEditableReplicationStreamModels were referenced by the UI so call this after clearing the UI.
		ConnectedState.Reset();
	}

	void FMultiUserReplicationManager::HandleReplicationSessionJoined(const ConcertSyncClient::Replication::FJoinReplicatedSessionResult& JoinSessionResult)
	{
		const bool bSuccess = JoinSessionResult.ErrorCode == EJoinReplicationErrorCode::Success;
		if (bSuccess)
		{
			ConnectedState.Emplace(Client);
			SetConnectionStateAndBroadcast(EMultiUserReplicationConnectionState::Connected);
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

	FMultiUserReplicationManager::FConnectedState::FConnectedState(TSharedRef<IConcertSyncClient> InClient)
		: ClientManager(InClient, InClient->GetConcertClient()->GetCurrentSession().ToSharedRef())
	{}
}
