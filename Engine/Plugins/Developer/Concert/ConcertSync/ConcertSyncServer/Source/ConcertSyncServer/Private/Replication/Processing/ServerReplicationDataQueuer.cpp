// Copyright Epic Games, Inc. All Rights Reserved.

#include "ServerReplicationDataQueuer.h"

#include "Replication/ConcertReplicationClient.h"
#include "Replication/Data/ReplicationStreamDescription.h"

namespace UE::ConcertSyncServer::Replication
{
	TSharedRef<FServerReplicationDataQueuer> FServerReplicationDataQueuer::Make(
		const FConcertReplicationClient& OwningClient,
		TSharedRef<ConcertSyncCore::FObjectReplicationCache> InReplicationCache
		)
	{
		TSharedRef<FServerReplicationDataQueuer> Result = MakeShared<FServerReplicationDataQueuer>(OwningClient);
		Result->BindToCache(MoveTemp(InReplicationCache));
		return Result;
	}

	bool FServerReplicationDataQueuer::WantsToAcceptObject(const ConcertSyncCore::FReplicationStreamObjectID& Object) const
	{
		// Do not send back the data to the client that generated it
		const bool bWasSentByThisClient = OwningClientStreamIds.Contains(Object.StreamId);
		
		// TODO: For now accept all objects from other clients. In the future, clients can specify what data they want to receive using client attributes.
		return !bWasSentByThisClient;
	}

	FServerReplicationDataQueuer::FServerReplicationDataQueuer(const FConcertReplicationClient& OwningClient)
		: OwningClientStreamIds([&OwningClient]()
		{
			TSet<FGuid> Streams;
			for (const FReplicationStreamDescription& Description : OwningClient.GetStreamDescriptions())
			{
				Streams.Add(Description.BaseDescription.Identifier);
			}
			return Streams;
		}())
	{}
}
