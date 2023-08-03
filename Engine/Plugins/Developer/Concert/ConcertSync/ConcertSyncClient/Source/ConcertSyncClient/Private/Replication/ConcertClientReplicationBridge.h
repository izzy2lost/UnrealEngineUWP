// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtr.h"
#include "Templates/SharedPointer.h"
#include "Replication/IConcertClientReplicationBridge.h"

class UObject;
class UWorld;
struct FWorldInitializationValues;

namespace UE::ConcertSyncClient::Replication
{
	class FConcertClientReplicationBridge : public IConcertClientReplicationBridge
	{
	public:
		
		FConcertClientReplicationBridge();
		virtual ~FConcertClientReplicationBridge() override;

		//~ Begin IConcertClientReplicationBridge Interface
		virtual void PushTrackedObjects(TArrayView<const FSoftObjectPath> InTrackedObjects) override;
		virtual void PopTrackedObjects(TArrayView<const FSoftObjectPath> InTrackedObjects) override;
		virtual bool IsObjectAvailable(const FSoftObjectPath& Path) override;
		virtual UObject* FindObjectIfAvailable(const FSoftObjectPath& Path) override;
		virtual FConcertClientReplicationBridgeObjectEvent& OnObjectDiscovered() override { return OnObjectDiscoveredDelegate; }
		virtual FConcertClientReplicationBridgeObjectEvent& OnObjectHidden() override { return OnObjectRemovedDelegate; }
		//~ End IConcertClientReplicationBridge Interface

	private:

		struct FTrackedObject
		{
			/** Tracks the number of PushTrackedObjects calls. This entry is removed upon reaching 0. */
			int32 TrackCounter = 0;
			/** Cached object pointer to avoid constant resolving of the soft object ptr. */
			TWeakObjectPtr<UObject> ResolvedObject;
		};

		/** The objects that wish to be tracked. */
		TMap<FSoftObjectPath, FTrackedObject> TrackedObjects;

		/** The set of worlds currently open on this client. */
		TSet<TWeakObjectPtr<UWorld>> LoadedWorlds;
	
		FConcertClientReplicationBridgeObjectEvent OnObjectDiscoveredDelegate;
		FConcertClientReplicationBridgeObjectEvent OnObjectRemovedDelegate;

		// Callbacks into the engine
		void OnPostWorldInitialization(UWorld* World, FWorldInitializationValues WorldInitializationValues);
		void OnPostWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	};
}

