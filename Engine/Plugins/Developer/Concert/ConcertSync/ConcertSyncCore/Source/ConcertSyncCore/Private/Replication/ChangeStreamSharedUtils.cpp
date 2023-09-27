// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ChangeStreamSharedUtils.h"

#include "Misc/EBreakBehavior.h"
#include "Replication/Data/ObjectIds.h"
#include "Replication/Messages/ConcertReplicationEvents.h"

namespace UE::ConcertSyncCore::Replication::ChangeStreamUtils
{
	void ForEachObjectLosingAuthority(
		const FConcertChangeStream_Request& Request,
		const TArray<FReplicationStreamDescription>& ExistingStreams,
		TFunctionRef<EBreakBehavior(const FObjectInStreamID&)> Callback
		)
	{
		for (const FReplicationStreamDescription& ExistingStream : ExistingStreams)
		{
			const FGuid& StreamId = ExistingStream.BaseDescription.Identifier;
			if (Request.StreamsToRemove.Contains(StreamId))
			{
				for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& RemovePair : ExistingStream.BaseDescription.ReplicationMap.ReplicatedObjects)
				{
					const FObjectInStreamID RemovedObjectId {  StreamId, RemovePair.Key };
					if (Callback(RemovedObjectId) == EBreakBehavior::Break)
					{
						return;
					}
				}
			}
		}

		// This will call Callback multiple times if the same object is in ObjectsToRemove and StreamsToRemove
		// but that does not really matter (it is a weird case anyhow - why would anyone construct such a request?)
		for (const FObjectInStreamID& ObjectToRemove : Request.ObjectsToRemove)
		{
			if (Callback(ObjectToRemove) == EBreakBehavior::Break)
			{
				return;
			}
		}
	}
	
	void ApplyValidatedRequest(const FConcertChangeStream_Request& Request, TArray<FReplicationStreamDescription>& StreamsToModify)
	{
		for (auto StreamIt = StreamsToModify.CreateIterator(); StreamIt; ++StreamIt)
		{
			FSharedReplicationStreamDescription& BaseDescription = StreamIt->BaseDescription;
			TMap<FSoftObjectPath, FReplicatedObjectInfo>& ReplicationMap = BaseDescription.ReplicationMap.ReplicatedObjects;
			const FGuid& StreamId = BaseDescription.Identifier;
			
			for (const FObjectInStreamID& ObjectToRemove : Request.ObjectsToRemove)
			{
				if (ObjectToRemove.StreamId == StreamId)
				{
					ReplicationMap.Remove(ObjectToRemove.Object);
				}
			}
			
			if (Request.StreamsToRemove.Contains(StreamId)
				// By contract, ObjectsToRemove will remove the stream if the stream ends up being empty
				|| ReplicationMap.IsEmpty())
			{
				StreamIt.RemoveCurrent();
			}
			
			for (const TPair<FObjectInStreamID, FConcertChangeStream_PutObject>& PutObjectPair : Request.ObjectsToPut)
			{
				if (PutObjectPair.Key.StreamId != StreamId)
				{
					continue;
				}

				const FSoftClassPath& PathToSet = PutObjectPair.Value.ClassPath;
				const FConcertPropertySelection& SelectionToSet = PutObjectPair.Value.Properties;
				FReplicatedObjectInfo& ObjectInfo = ReplicationMap.FindOrAdd(PutObjectPair.Key.Object);

				// Write ClassPath if FindOrAdd just added it or if request specified a value to overwrite with ...
				if (!ObjectInfo.ClassPath.IsValid() || PathToSet.IsValid())
				{
					ObjectInfo.ClassPath = PutObjectPair.Value.ClassPath;
				}
				// ... but one of these cases must occur
				checkf(ObjectInfo.ClassPath.IsValid(), TEXT("Request did not validate ClassPath!"));

				// Write Properties if FindOrAdd just added it or if request specified a value to overwrite with ...
				if (ObjectInfo.PropertySelection.ReplicatedProperties.IsEmpty() || !SelectionToSet.ReplicatedProperties.IsEmpty())
				{
					ObjectInfo.PropertySelection = SelectionToSet;
				}
				// ... but one of these cases must occur
				checkf(ObjectInfo.ClassPath.IsValid(), TEXT("Request did not validate Properties!"));
			}
		}
		
		for (const FReplicationStreamDescription_NetPacked& StreamDescription : Request.StreamsToAdd)
		{
			if (const TOptional<FReplicationStreamDescription> Unpacked = StreamDescription.Unpack()
				; ensureMsgf(Unpacked, TEXT("Request did not validate unpacking!")))
			{
				StreamsToModify.Add(*Unpacked);
			}
		}
	}
	
	void IterateInvalidEntries(
		const FObjectReplicationMap& ReplicationMap,
		TFunctionRef<EBreakBehavior(const FSoftObjectPath&, const FReplicatedObjectInfo&)> Callback
		)
	{
		for (const TPair<const FSoftObjectPath&, const FReplicatedObjectInfo&> Pair : ReplicationMap.ReplicatedObjects)
		{
			if (!Pair.Value.IsValidForSendingToServer() && Callback(Pair.Key, Pair.Value) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	namespace Private
	{
		static void BuildPutObjectList(const FGuid& StreamId, const FObjectReplicationMap& Base, const FObjectReplicationMap& Desired, FConcertChangeStream_Request& Request)
		{
			for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& BasePair : Base.ReplicatedObjects)
			{
				const FSoftObjectPath& ObjectPath = BasePair.Key;
				const FObjectInStreamID ObjectId { StreamId, ObjectPath };
				const FReplicatedObjectInfo* DesiredObjectInfo = Desired.ReplicatedObjects.Find(ObjectPath);
				if (DesiredObjectInfo)
				{
					const FReplicatedObjectInfo& BaseObjectInfo = BasePair.Value;
					const TOptional<FConcertChangeStream_PutObject> PutObject = FConcertChangeStream_PutObject::MakeFromChange(BaseObjectInfo, *DesiredObjectInfo);
				
					const bool bDesiredHasChangedFromBase = BaseObjectInfo != *DesiredObjectInfo;
					// If MakeFromChange returned unset, it means that this request is not valid to submit. 
					const bool bBaseAndDesiredStateAreValid = ensureMsgf(PutObject, TEXT("Function assumption violated; you did not pass in valid base or desired state."));
					if (bDesiredHasChangedFromBase && bBaseAndDesiredStateAreValid)
					{
						Request.ObjectsToPut.Add(ObjectId, *PutObject);
					}
				}
				else
				{
					Request.ObjectsToRemove.Add(ObjectId);
				}
			}
		}

		static void BuildRemoveObjectList(const FGuid& StreamId, const FObjectReplicationMap& Base, const FObjectReplicationMap& Desired, FConcertChangeStream_Request& Request)
		{
			for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& DesiredPair : Desired.ReplicatedObjects)
			{
				const FSoftObjectPath& ObjectPath = DesiredPair.Key;
				if (Base.ReplicatedObjects.Contains(ObjectPath))
				{
					// Handled up above
					continue;
				}

				// Desired wants to add an object
				const FReplicatedObjectInfo& ObjectInfo = DesiredPair.Value;
				const TOptional<FConcertChangeStream_PutObject> PutObject = FConcertChangeStream_PutObject::MakeFromInfo(ObjectInfo);
				const bool bDesiredStateHasEnoughDataToForPut = ensureMsgf(PutObject, TEXT("Function assumption violated; you did not pass in valid base or desired state."));
				if (bDesiredStateHasEnoughDataToForPut)
				{
					Request.ObjectsToPut.Add({ StreamId, ObjectPath}, *PutObject);
				}
			}
		}
	}

	FConcertChangeStream_Request BuildRequestFromDiff(
		const FGuid& StreamId,
		const FObjectReplicationMap& Base,
		const FObjectReplicationMap& Desired
		)
	{
		FConcertChangeStream_Request Request;
		Private::BuildPutObjectList(StreamId, Base, Desired, Request);
		Private::BuildRemoveObjectList(StreamId, Base, Desired, Request);
		return Request;
	}
}
