// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Processing/ObjectReplicationSender.h"

#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Algo/Accumulate.h"
#include "Replication/Processing/IReplicationDataSource.h"

namespace UE::ConcertSyncCore
{
	FObjectReplicationSender::FObjectReplicationSender(
		const FGuid& TargetEndpointId,
		TSharedRef<IConcertSession> Session,
		TSharedRef<IReplicationDataSource> DataSource
		)
		: FObjectReplicationProcessor(MoveTemp(DataSource))
		, TargetEndpointId(TargetEndpointId)
		, Session(MoveTemp(Session))
	{}

	void FObjectReplicationSender::ProcessObjects(float TimeBudget)
	{
		FObjectReplicationProcessor::ProcessObjects(TimeBudget);

		if (!EventToSend.Streams.IsEmpty())
		{
			const int32 NumObjects = Algo::TransformAccumulate(EventToSend.Streams, [](const FConcertStreamReplicationEvent& Event){ return Event.ReplicatedObjects.Num(); }, 0);
			UE_LOG(LogConcert, Verbose, TEXT("Sending %d streams with %d objects to %s"),
				EventToSend.Streams.Num(),
				NumObjects,
				*TargetEndpointId.ToString()
				);
			
			Session->SendCustomEvent(EventToSend, TargetEndpointId,
				// Replication is always unreliable - if it fails to deliver we'll send updated data soon again
				// TODO: In regular intervals send CRC values to detect that a change is missing
				EConcertMessageFlags::None
			);
			EventToSend.Streams.Empty(
				// It's not unreasonable to expect the next pass to have a similar number of objects so keep it around to avoid re-allocating all the time
				EventToSend.Streams.Num()
				);
		}
	}

	void FObjectReplicationSender::ProcessObject(const FObjectProcessArgs& Args)
	{
		const FSoftObjectPath& ReplicatedObject = Args.ObjectInfo.Object;
		auto CaptureData = [this, &Args, &ReplicatedObject]<typename TPayloadRefType>(TPayloadRefType&& Payload)
		{
			const int32 PreexistingIndex = EventToSend.Streams.IndexOfByPredicate([&Args](const FConcertStreamReplicationEvent& StreamData){ return StreamData.StreamId == Args.ObjectInfo.StreamId; });
			FConcertStreamReplicationEvent& StreamData = EventToSend.Streams.IsValidIndex(PreexistingIndex)
				? EventToSend.Streams[PreexistingIndex]
				: EventToSend.Streams[EventToSend.Streams.Emplace(Args.ObjectInfo.StreamId)];
			StreamData.ReplicatedObjects.Add({
				ReplicatedObject,
				// Take advantage of move semantics if it is possible - this depends on how our data source internally obtains its payloads
				Forward<TPayloadRefType>(Payload)
			});
		};
		GetDataSource().ExtractReplicationDataForObject(Args.ObjectInfo, CaptureData, CaptureData);
	}
}
