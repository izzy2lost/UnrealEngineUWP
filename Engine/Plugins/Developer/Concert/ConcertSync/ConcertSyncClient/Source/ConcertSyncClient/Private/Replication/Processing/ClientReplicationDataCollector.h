// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Processing/IReplicationDataSource.h"

#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Replication/Data/ConcertPropertySelection.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPtr.h"

class FConcertSyncClientLiveSession;
class IConcertClientReplicationBridge;
class UObject;
struct FConcertPropertySelection;
struct FReplicationStreamDescription;

namespace UE::ConcertSyncCore
{
	class IObjectReplicationFormat;
	class FObjectReplicationProcessor;
}

namespace UE::ConcertSyncClient::Replication
{
	
	/**
	 * Exposes UObject instances to an FObjectReplicationProcessor.
	 * IConcertClientReplicationBridge tracks UObject lifetime, this class exposes them.
	 */
	class FClientReplicationDataCollector : public ConcertSyncCore::IReplicationDataSource
	{
	public:
		
		FClientReplicationDataCollector(
			IConcertClientReplicationBridge* ReplicationBridge,
			TSharedRef<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat,
			TArrayView<FReplicationStreamDescription> StreamsToSend
			);
		virtual ~FClientReplicationDataCollector() override;

		//~ Begin IReplicationDataSource Interface
		virtual void ForEachPendingObject(TFunctionRef<void(const ConcertSyncCore::FReplicationStreamObjectID&)> ProcessItemFunc) const override;
		virtual int32 NumObjects() const override { return NumTrackedObjects; }
		virtual bool ExtractReplicationDataForObject(const ConcertSyncCore::FReplicationStreamObjectID& Object, TFunctionRef<void(const FConcertSessionSerializedPayload& Payload)> ProcessCopyable, TFunctionRef<void(FConcertSessionSerializedPayload&& Payload)> ProcessMoveable) override;
		//~ End IReplicationDataSource Interface

	private:

		/** Gets and tracks replicated objects */
		IConcertClientReplicationBridge* Bridge;
		/** Used to create the replication data sent to the server. */
		TSharedRef<ConcertSyncCore::IObjectReplicationFormat> ReplicationFormat;

		struct FObjectInfo
		{
			/** The replication stream producing this object's data */
			FGuid StreamId;
			/** The properties to replicate */
			FConcertPropertySelection SelectedProperties;
			/** Set when the bridge tells us the object is available for replication. Unset otherwise. */
			TWeakObjectPtr<UObject> ObjectCache;
		};
		
		/** The objects and their properties to replicate */
		TMap<FSoftObjectPath, TArray<FObjectInfo>> ObjectsToReplicate;
		/** Cached number of FObjectInfo in ObjectsToReplicate that have a valid FObjectInfo::ObjectCache. */
		int32 NumTrackedObjects = 0;

		// Handle events from bridge
		void StartTrackingObject(UObject& Object);
		void StopTrackingObject(UObject& Object);
	};
}
