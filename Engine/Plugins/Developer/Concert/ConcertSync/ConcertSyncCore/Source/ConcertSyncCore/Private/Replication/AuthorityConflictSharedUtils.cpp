// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/AuthorityConflictSharedUtils.h"

#include "Replication/Data/ObjectIds.h"
#include "Replication/Data/ReplicationStreamDescription.h"

namespace UE::ConcertSyncCore::Replication::AuthorityConflictUtils
{
	namespace Private
	{
		static void ForEachClientWithPotentialConflict(
            const FSoftObjectPath& Object,
            TFunctionRef<EBreakBehavior(const FGuid& ClientId, const FGuid& StreamId, const FConcertPropertySelection& ConflictingProperty)> Callback,
            TArrayView<const FGuid> IgnoredClients,
            const IReplicationGroundTruth& GroundTruth
            )
		{
			GroundTruth.ForEachSendingClient([&Object, &Callback, &IgnoredClients, &GroundTruth](const FGuid& ClientEndpointId)
			{
				if (IgnoredClients.Contains(ClientEndpointId))
				{
					return EBreakBehavior::Continue;
				}
				
				EBreakBehavior Result = EBreakBehavior::Continue;
				GroundTruth.ForEachStream(ClientEndpointId, [&Object, &Callback, &ClientEndpointId, &GroundTruth, &Result](const FSharedReplicationStreamDescription& Stream) mutable
				{
					// If client has not claimed authority over this object in this stream, skip
					const FGuid& StreamId = Stream.Identifier;
					if (!GroundTruth.HasAuthority(ClientEndpointId, StreamId, Object))
					{
						return EBreakBehavior::Continue;
					}

					// This client is using the request object: report the potential conflict ...
					const FObjectReplicationMap& ObjectReplicationMap = Stream.ReplicationMap;
					const FReplicatedObjectInfo* ReplicationObjectInfo = ObjectReplicationMap.ReplicatedObjects.Find(Object);
					if (ReplicationObjectInfo && Callback(ClientEndpointId, StreamId, ReplicationObjectInfo->PropertySelection) == EBreakBehavior::Break)
					{
						// ... conflict ends iteration
						Result = EBreakBehavior::Break;
						return EBreakBehavior::Break;
					}
					// ... conflict resolved
					return EBreakBehavior::Continue;
				});
				return Result;
			});
		}
	}
	
	EAuthorityConflict EnumerateAuthorityConflicts(
		const FGuid& ClientId,
		const FSoftObjectPath& Object,
		TConstArrayView<FConcertPropertyChain> OverwriteProperties,
		const IReplicationGroundTruth& GroundTruth,
		FProcessAuthorityConflict ProcessConflict
		)
	{
		const FGuid IgnoredClients[] = { ClientId };

		bool bFreeOfConflicts = true;
		Private::ForEachClientWithPotentialConflict(
			Object,
			[&ProcessConflict, &OverwriteProperties, &bFreeOfConflicts](const FGuid& ClientId, const FGuid& StreamId, const FConcertPropertySelection& WrittenProperties) mutable
			{
				EBreakBehavior BreakBehavior = EBreakBehavior::Continue;
				const bool bHasOverlap = FConcertPropertySelection::EnumeratePropertyOverlaps(WrittenProperties.ReplicatedProperties, OverwriteProperties,
					[&ProcessConflict, &ClientId, &StreamId, &BreakBehavior](const FConcertPropertyChain& Overlap)
					{
						BreakBehavior = ProcessConflict(ClientId, StreamId, Overlap);
						return BreakBehavior;
					});
				
				// At this point we know that WritingClientId is sending WrittenProperties - if the properties overlap, the authority request is not possible
				const bool bHasNoConflict = !bHasOverlap;
				bFreeOfConflicts &= bHasNoConflict;
				return BreakBehavior;
			},
			IgnoredClients,
			GroundTruth
			);
		return bFreeOfConflicts ? EAuthorityConflict::Allowed : EAuthorityConflict::Conflict;
	}
}
