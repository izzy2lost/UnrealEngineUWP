// Copyright Epic Games, Inc. All Rights Reserved.

#include "StreamRequestUtils.h"

#include "Replication/Stream/IClientStreamSynchronizer.h"
#include "Replication/Stream/StreamChangeTracker.h"

namespace UE::MultiUserClient::StreamRequestUtils
{
	FConcertReplication_ChangeStream_Request BuildChangeRequest_CreateNewStream(const FGuid& StreamId, const FStreamChangelist& FromChangelist)
	{
		const TMap<FObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& ObjectsToPut = FromChangelist.ObjectsToPut;
		
		FConcertReplication_ChangeStream_Request Request;
		Request.StreamsToAdd.Emplace();
		FReplicationStreamDescription_NetPacked& NewStream = Request.StreamsToAdd[0];
		NewStream.BaseDescription.Identifier = StreamId;
			
		// If creating a new stream, the objects must be supplied in the description instead of in PutObjects!
		FObjectReplicationMap& ReplicationMap = NewStream.BaseDescription.ReplicationMap;
		ReplicationMap.ReplicatedObjects.Reserve(ObjectsToPut.Num());
		for (const TPair<FObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& PutObjectPair : ObjectsToPut)
		{
			const TOptional<FReplicatedObjectInfo> NewObjectInfo = PutObjectPair.Value.MakeObjectInfoIfValid();
			// The editing UI allows adding objects without properties (to make UX easier) - do not submit those to the server.
			if (!NewObjectInfo)
			{
				continue;
			}
			checkf(PutObjectPair.Key.StreamId == StreamId, TEXT("BuildChangeRequest should always use LocalClientStreamId ID!"));
				
			ReplicationMap.ReplicatedObjects.Add(PutObjectPair.Key.Object, *NewObjectInfo);
		}
		
		return Request;
	}
		
	FConcertReplication_ChangeStream_Request BuildChangeRequest_UpdateExistingStream(FStreamChangelist FromChangelist)
	{
		return { MoveTemp(FromChangelist.ObjectsToRemove), MoveTemp(FromChangelist.ObjectsToPut) };
	}
}
