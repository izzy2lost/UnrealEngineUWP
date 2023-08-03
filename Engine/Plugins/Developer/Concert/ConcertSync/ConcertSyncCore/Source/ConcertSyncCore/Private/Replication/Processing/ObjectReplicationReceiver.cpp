// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ObjectReplicationReceiver.h"

#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/Messages/ConcertReplicationEvents.h"
#include "Replication/Processing/ObjectReplicationCache.h"

namespace UE::ConcertSyncCore
{
	FObjectReplicationReceiver::FObjectReplicationReceiver(TSharedRef<IConcertSession> Session, TSharedRef<FObjectReplicationCache> ReplicationCache)
		: Session(MoveTemp(Session))
		, ReplicationCache(MoveTemp(ReplicationCache))
	{
		Session->RegisterCustomEventHandler<FConcertBatchReplicationEvent>(this, &FObjectReplicationReceiver::HandleBatchReplicationEvent);
	}

	FObjectReplicationReceiver::~FObjectReplicationReceiver()
	{
		Session->UnregisterCustomEventHandler<FConcertBatchReplicationEvent>(this);
	}

	void FObjectReplicationReceiver::HandleBatchReplicationEvent(const FConcertSessionContext& SessionContext, const FConcertBatchReplicationEvent& Event)
	{
		int32 NumObjects = 0;
		int32 NumCacheUsages = 0;
		int32 NumOfAcceptedObjects = 0;
		
		for (const FConcertStreamReplicationEvent& StreamEvent : Event.Streams)
		{
			NumObjects += StreamEvent.ReplicatedObjects.Num();
			for (const FConcertObjectReplicationEvent& ObjectEvent : StreamEvent.ReplicatedObjects)
			{
				const int32 NumAccepted = ReplicationCache->StoreUntilConsumed(StreamEvent.StreamId, ObjectEvent);
				NumCacheUsages += NumAccepted;
				NumOfAcceptedObjects += NumAccepted == 0 ? 0 : 1;
			}
		}
		
		UE_LOG(LogConcert, Verbose, TEXT("Received %d streams with %d objects from endpoint %s. Cached %d objects with a total of %d cache usages."),
			Event.Streams.Num(),
			NumObjects,
			*SessionContext.SourceEndpointId.ToString(),
			NumOfAcceptedObjects,
			NumCacheUsages
			);
	}
}
