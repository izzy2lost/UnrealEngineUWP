// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationManager.h"

#include "IConcertSyncClient.h"
#include "ReplicationUtils.h"

#include "UObject/Package.h"

namespace UE::MultiUserClient
{
	FMultiUserReplicationManager::FMultiUserReplicationManager(TSharedRef<IConcertSyncClient> InClient)
		: Client(MoveTemp(InClient))
		, SessionContent(NewObject<UMultiUserReplicationSessionPreset>(GetTransientPackage(), NAME_None, RF_Transient))
		, LocalClientContent(SessionContent->AddClient())
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

	void FMultiUserReplicationManager::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(SessionContent);
		Collector.AddReferencedObject(LocalClientContent);
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
			OnJoinSession(ConcertClientSession);
			break;
		case EConcertConnectionStatus::Disconnecting:
			break;
		case EConcertConnectionStatus::Disconnected:
			OnLeaveSession(ConcertClientSession);
			break;
		default: ;
		}
	}

	void FMultiUserReplicationManager::OnJoinSession(IConcertClientSession& ConcertClientSession)
	{
		Replication::JoinSessionForMultiUser(Client, {});
	}

	void FMultiUserReplicationManager::OnLeaveSession(IConcertClientSession& ConcertClientSession)
	{
		ClearSessionData();
		
		if (IConcertClientReplicationManager* ReplicationManager = Client->GetReplicationManager())
		{
			ReplicationManager->LeaveReplicationSession();
		}
	}

	void FMultiUserReplicationManager::ClearSessionData()
	{
		SessionContent->ClearClients();
		LocalClientContent = SessionContent->AddClient();
	}
}
