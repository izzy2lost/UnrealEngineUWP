// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/AnyOf.h"
#include "Replication/IConcertClientReplicationBridge.h"

namespace UE::ConcertSyncTests
{
	class FConcertClientReplicationBridgeMock : public IConcertClientReplicationBridge
	{
	public:

		TArray<FSoftObjectPath> TrackedObjects;
		TArray<UObject*> AvailableObjects;
		
		FConcertClientReplicationBridgeObjectEvent OnObjectDiscoveredDelegate;
		FConcertClientReplicationBridgeObjectEvent OnObjectRemovedDelegate;

		void InjectAvailableObject(UObject& Object)
		{
			AvailableObjects.Add(&Object);
			OnObjectDiscoveredDelegate.Broadcast(Object);
		}
		
		virtual void PushTrackedObjects(TArrayView<const FSoftObjectPath> InTrackedObjects) override
		{
			for (const FSoftObjectPath& Path : InTrackedObjects)
			{
				TrackedObjects.Add(Path);
			}
		}
		
		virtual void PopTrackedObjects(TArrayView<const FSoftObjectPath> InTrackedObjects) override
		{
			for (const FSoftObjectPath& Path : InTrackedObjects)
			{
				TrackedObjects.RemoveSingle(Path);
			}
		}

		virtual bool IsObjectAvailable(const FSoftObjectPath& Path) override { return Algo::AnyOf(AvailableObjects, [&Path](UObject* Object){ return Path == Object; }); }
		virtual UObject* FindObjectIfAvailable(const FSoftObjectPath& Path) override
		{
			for (UObject* Object : AvailableObjects)
			{
				if (Path == Object)
				{
					return Object;
				}
			}
			return nullptr;
		}

		virtual FConcertClientReplicationBridgeObjectEvent& OnObjectDiscovered() override { return OnObjectDiscoveredDelegate; }
		virtual FConcertClientReplicationBridgeObjectEvent& OnObjectHidden() override { return OnObjectRemovedDelegate; }
	};
}
