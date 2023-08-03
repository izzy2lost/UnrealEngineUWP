// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertClientReplicationBridge.h"

#include "ConcertLogGlobal.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/World.h"

class IAssetRegistry;

namespace UE::ConcertSyncClient::Replication
{
	FConcertClientReplicationBridge::FConcertClientReplicationBridge()
	{
		FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FConcertClientReplicationBridge::OnPostWorldInitialization);
		FWorldDelegates::OnPostWorldCleanup.AddRaw(this, &FConcertClientReplicationBridge::OnPostWorldCleanup);
	}

	FConcertClientReplicationBridge::~FConcertClientReplicationBridge()
	{
		FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
		FWorldDelegates::OnPostWorldCleanup.RemoveAll(this);
	}

	void FConcertClientReplicationBridge::PushTrackedObjects(TArrayView<const FSoftObjectPath> InTrackedObjects)
	{
		TSet<UObject*> ResolvedObjects;
		
		for (const FSoftObjectPath& TrackedObjectPath : InTrackedObjects)
		{
			FTrackedObject& TrackedObject = TrackedObjects.FindOrAdd(TrackedObjectPath);
			++TrackedObject.TrackCounter;
			if (!TrackedObject.ResolvedObject.IsValid())
			{
				UObject* ResolvedObject = FindObjectIfAvailable(TrackedObjectPath);
				TrackedObject.ResolvedObject = ResolvedObject;
				if (ResolvedObject)
				{
					ResolvedObjects.Add(ResolvedObject);
				}
			}
		}
		
		// Broadcast after updating TrackedObjects in the unlikely case that the broadcast calls PushTrackedObjects or PopTrackedObjects.
		for (UObject* ResolvedObject : ResolvedObjects)
		{
			OnObjectDiscoveredDelegate.Broadcast(*ResolvedObject);
		}
	}

	void FConcertClientReplicationBridge::PopTrackedObjects(TArrayView<const FSoftObjectPath> InTrackedObjects)
	{
		for (const FSoftObjectPath& TrackedObjectPath : InTrackedObjects)
		{
			FTrackedObject* TrackedObject = TrackedObjects.Find(TrackedObjectPath);
			if (!TrackedObject)
			{
				UE_LOG(LogConcert, Warning, TEXT("Object %s was requested to not be tracked but was it already wasn't."), *TrackedObjectPath.ToString());
				continue;
			}

			if (TrackedObject->TrackCounter == 1)
			{
				TrackedObjects.Remove(TrackedObjectPath);
			}
			else
			{
				--TrackedObject->TrackCounter;
			}
		}
	}

	bool FConcertClientReplicationBridge::IsObjectAvailable(const FSoftObjectPath& Path)
	{
		// TODO: Possibly iterate through worlds and see whether they're the parent of this object. Profile performance.
		return FindObjectIfAvailable(Path) != nullptr;
	}

	UObject* FConcertClientReplicationBridge::FindObjectIfAvailable(const FSoftObjectPath& Path)
	{
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry)
		{
			return nullptr;
		}

		constexpr bool bSkipInMemory = true;
		const FAssetData AssetData = AssetRegistry->GetAssetByObjectPath(Path, bSkipInMemory);
		const bool bIsAsset = AssetData.IsValid() || Path.IsAsset();
		if (!ensure(!bIsAsset))
		{
			UE_LOG(LogConcert, Error, TEXT("Assets are not supported for replication in this version (%s)"), *Path.ToString());
			return nullptr;
		}
		
		// This should be an object in a UWorld. This will resolve if the world is opened.
		return Path.ResolveObject();
	}

	void FConcertClientReplicationBridge::OnPostWorldInitialization(UWorld* World, FWorldInitializationValues WorldInitializationValues)
	{
		// We'll allow PIE even though it is currently not explicitly supported; the primary use case is editor or game worlds (if someone tries it in packaged).
		if (!ensure(World) || World->IsPreviewWorld())
		{
			return;
		}
		
		LoadedWorlds.Add(World);

		// TODO: Discover objects
	}

	void FConcertClientReplicationBridge::OnPostWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		const int32 NumRemoved = LoadedWorlds.Remove(World);
		bool bRemovedAnything = NumRemoved > 0;
		if (!bRemovedAnything)
		{
			return;
		}

		// TODO: Hide objects
	}
}
