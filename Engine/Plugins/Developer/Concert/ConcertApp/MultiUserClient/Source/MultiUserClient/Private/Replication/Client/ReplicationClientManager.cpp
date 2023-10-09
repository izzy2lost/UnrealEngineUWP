// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClientManager.h"

#include "Assets/MultiUserReplicationSessionPreset.h"
#include "Replication/Stream/LocalClientStreamSynchronizer.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::MultiUserClient
{
	FReplicationClientManager::FReplicationClientManager(TSharedRef<IConcertSyncClient> InClient)
		: SessionContent(NewObject<UMultiUserReplicationSessionPreset>(GetTransientPackage(), NAME_None, RF_Transient)) 
		, LocalClient([this, &InClient]()
		{
			UMultiUserReplicationClientPreset* ClientPreset = SessionContent->AddClient();
			return FReplicationClient(*ClientPreset, InClient, MakeShared<FLocalClientStreamSynchronizer>(InClient, ClientPreset->Stream->StreamId));
		}())
	{}

	void FReplicationClientManager::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(SessionContent);
	}
}
