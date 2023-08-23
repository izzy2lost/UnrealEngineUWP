// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ObjectReplicationReceiver.h"

#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/Messages/ConcertReplicationEvents.h"
#include "Replication/Processing/ObjectReplicationCache.h"

#include "HAL/IConsoleManager.h"

namespace UE::ConcertSyncCore
{
	static TAutoConsoleVariable<bool> CVarLogReceivedObjects(TEXT("Concert.Replication.LogReceivedObjects"), false, TEXT("Enable Concert logging for received replicated objects."));
	
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
		// Fyi: an object may have multiple changes in a batch replication event: each stream can modify different properties as long as they do not overlap.
		int32 NumObjects = 0;
		int32 NumRejectedObjectChanges = 0;
		int32 NumCacheUsages = 0;
		int32 NumOfAcceptedObjectChanges = 0;
		
		for (const FConcertStreamReplicationEvent& StreamEvent : Event.Streams)
		{
			const int32 ObjectsInStream = StreamEvent.ReplicatedObjects.Num();
			NumObjects += ObjectsInStream;
			
			for (const FConcertObjectReplicationEvent& ObjectEvent : StreamEvent.ReplicatedObjects)
			{
				if (ShouldAcceptObject(SessionContext, StreamEvent, ObjectEvent))
				{
					const int32 NumAccepted = ReplicationCache->StoreUntilConsumed(SessionContext.SourceEndpointId, StreamEvent.StreamId, ObjectEvent);
					NumCacheUsages += NumAccepted;
					NumOfAcceptedObjectChanges += NumAccepted == 0 ? 0 : 1;
				}
				else
				{
					++NumRejectedObjectChanges;
				}
			}
		}
		
		UE_CLOG(CVarLogReceivedObjects.GetValueOnGameThread(), LogConcert, Log, TEXT("Received %d streams with %d object changes from endpoint %s. Rejected %d object changes. Cached %d object changes with a total of %d cache usages."),
			Event.Streams.Num(),
			NumObjects,
			*SessionContext.SourceEndpointId.ToString(),
			NumRejectedObjectChanges,
			NumOfAcceptedObjectChanges,
			NumCacheUsages
			);
	}
}
