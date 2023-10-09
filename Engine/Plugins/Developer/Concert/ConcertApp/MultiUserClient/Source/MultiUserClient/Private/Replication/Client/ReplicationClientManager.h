// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationClient.h"
#include "Assets/MultiUserReplicationSessionPreset.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"
#include "UObject/GCObject.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::ConcertClientSharedSlate
{
	class IEditableObjectToPropertiesModel;
}

namespace UE::MultiUserClient
{
	class FReplicationClient;
	class FLocalStreamChangeTracker;

	/** Keeps track of connected clients synchronizing their stream data in a UMultiUserReplicationSessionPreset. */
	class FReplicationClientManager
		: public FGCObject
		, public FNoncopyable
	{
	public:

		FReplicationClientManager(TSharedRef<IConcertSyncClient> InClient);

		FReplicationClient& GetLocalClient() { return LocalClient; }
		
		//~ Begin FGCObject Interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FReplicationStreamSynchronizer"); }
		//~ End FGCObject Interface
		
	private:
		
		/** The state of the server is synched up with this object and displayed in the UI. */
		TObjectPtr<UMultiUserReplicationSessionPreset> SessionContent;

		/** Manages the local client */
		FReplicationClient LocalClient;
	};
}

