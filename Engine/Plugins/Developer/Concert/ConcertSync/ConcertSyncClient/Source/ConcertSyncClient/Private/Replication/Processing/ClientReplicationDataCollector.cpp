// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientReplicationDataCollector.h"

#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/Formats/IObjectReplicationFormat.h"
#include "Replication/IConcertClientReplicationBridge.h"
#include "Replication/ReplicationPropertyFilter.h"
#include "Replication/ReplicationStreamObjectID.h"

namespace UE::ConcertSyncClient::Replication
{
	FClientReplicationDataCollector::FClientReplicationDataCollector(
		IConcertClientReplicationBridge* ReplicationBridge,
		TSharedRef<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat,
		TArrayView<FReplicationStreamDescription> StreamDescriptions
		)
		: Bridge(ReplicationBridge)
		, ReplicationFormat(MoveTemp(ReplicationFormat))
	{
		Bridge->OnObjectDiscovered().AddRaw(this, &FClientReplicationDataCollector::StartTrackingObject);
		Bridge->OnObjectHidden().AddRaw(this, &FClientReplicationDataCollector::StopTrackingObject);

		TSet<FSoftObjectPath> PushedSet;
		for (const FReplicationStreamDescription& StreamDescription : StreamDescriptions)
		{
			const FGuid StreamId = StreamDescription.BaseDescription.Identifier;
			for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& Pair : StreamDescription.BaseDescription.ReplicationMap.ReplicatedObjects)
			{
				const FSoftObjectPath& ObjectPath = Pair.Key;
				
				TArray<FObjectInfo>& ExistingObjectInfo = ObjectsToReplicate.FindOrAdd(ObjectPath);
				checkf(!ExistingObjectInfo.ContainsByPredicate([&StreamId](const FObjectInfo& ObjectInfo){ return ObjectInfo.StreamId == StreamId; }), TEXT("Invalid data should have been rejected during handshake"));

				ExistingObjectInfo.Add({ StreamId, Pair.Value.PropertySelection });
				// Only push it once because each push increments a counter
				if (!PushedSet.Contains(ObjectPath))
				{
					PushedSet.Add(ObjectPath);
					Bridge->PushTrackedObjects({ ObjectPath });
				}
			}
		}
	}

	FClientReplicationDataCollector::~FClientReplicationDataCollector()
	{
		Bridge->OnObjectDiscovered().RemoveAll(this);
		Bridge->OnObjectHidden().RemoveAll(this);

		for (const TPair<FSoftObjectPath, TArray<FObjectInfo>>& Pair : ObjectsToReplicate)
		{
			Bridge->PopTrackedObjects({ Pair.Key });
		}
	}

	void FClientReplicationDataCollector::ForEachPendingObject(TFunctionRef<void(const ConcertSyncCore::FReplicationStreamObjectID&)> ProcessItemFunc) const
	{
		for (const TPair<FSoftObjectPath, TArray<FObjectInfo>>& Pair : ObjectsToReplicate)
		{
			for (const FObjectInfo& ObjectInfo : Pair.Value)
			{
				if (ObjectInfo.ObjectCache.IsValid())
				{
					ProcessItemFunc({ ObjectInfo.StreamId, ObjectInfo.ObjectCache.Get() });
				}
			}
		}
	}

	bool FClientReplicationDataCollector::ExtractReplicationDataForObject(
		const ConcertSyncCore::FReplicationStreamObjectID& ObjectToProcess,
		TFunctionRef<void(const FConcertSessionSerializedPayload& Payload)> ProcessCopyable,
		TFunctionRef<void(FConcertSessionSerializedPayload&& Payload)> ProcessMoveable
		)
	{
		const TArray<FObjectInfo>* ObjectInfos = ObjectsToReplicate.Find(ObjectToProcess.Object);
		// ExtractReplicationDataForObject is supposed to be called in response to ForEachPendingObject... so either the call was invalid or ForEachPendingObject lied
		if (!ensure(ObjectInfos))
		{
			return {};
		}

		const int32 Index = ObjectInfos->IndexOfByPredicate([&ObjectToProcess](const FObjectInfo& ObjectInfo){ return ObjectInfo.StreamId == ObjectToProcess.StreamId; });
		// Same logic as above - either invalid call or ForEachPendingObject lied
		if (!ensure(ObjectInfos->IsValidIndex(Index)))
		{
			return {};
		}

		const FObjectInfo& ObjectInfo = (*ObjectInfos)[Index];
		// Finally... same logic as above - either invalid call or ForEachPendingObject lied
		if (!ensure(ObjectInfo.ObjectCache.IsValid()))
		{
			return false;
		}

		ConcertSyncCore::FReplicationPropertyFilter Filter(ObjectInfo.SelectedProperties);
		TOptional<FConcertSessionSerializedPayload> Payload = ReplicationFormat->CreateReplicationEvent(
			*ObjectInfo.ObjectCache,
			[&Filter](const FArchiveSerializedPropertyChain* Chain, const FProperty& Property)
			{
				return Filter.ShouldSerializeProperty(Chain, Property);
			}
		);
		if (Payload)
		{
			ProcessMoveable(MoveTemp(*Payload));
		}
		return true;
	}

	void FClientReplicationDataCollector::StartTrackingObject(UObject& Object)
	{
		if (TArray<FObjectInfo>* ObjectInfos = ObjectsToReplicate.Find(&Object))
		{
			for (FObjectInfo& ObjectInfo : *ObjectInfos)
			{
				ObjectInfo.ObjectCache = &Object;
			}
			++NumTrackedObjects;
		}
	}

	void FClientReplicationDataCollector::StopTrackingObject(UObject& Object)
	{
		if (TArray<FObjectInfo>* ObjectInfos = ObjectsToReplicate.Find(&Object))
		{
			for (FObjectInfo& ObjectInfo : *ObjectInfos)
			{
				ObjectInfo.ObjectCache = nullptr;
			}
			--NumTrackedObjects;
		}
	}
}
