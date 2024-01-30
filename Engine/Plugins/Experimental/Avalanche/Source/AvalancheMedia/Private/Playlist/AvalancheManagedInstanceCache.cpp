// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvalancheManagedInstanceCache.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AvalancheMediaSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IAvaMediaModule.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Playlist/AvalancheManagedInstanceBlueprint.h"
#include "Playlist/AvalancheManagedInstanceLevel.h"
#include "UObject/Package.h"

FAvalancheManagedInstanceCache::FAvalancheManagedInstanceCache()
{
	UPackage::PackageSavedWithContextEvent.AddRaw(this, &FAvalancheManagedInstanceCache::OnPackageSaved);
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnAssetRemoved().AddRaw(this, &FAvalancheManagedInstanceCache::OnAssetRemoved);
	}
	IAvaMediaModule::Get().GetOnAvaMediaSyncPackageModified().AddRaw(this, &FAvalancheManagedInstanceCache::OnAvaSyncPackageModified);
	
#if WITH_EDITOR
	UAvalancheMediaSettings::GetMutable().OnSettingChanged().AddRaw(this, &FAvalancheManagedInstanceCache::OnSettingChanged);	
#endif
}

FAvalancheManagedInstanceCache::~FAvalancheManagedInstanceCache()
{
	UPackage::PackageSavedWithContextEvent.RemoveAll(this);
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnAssetRemoved().RemoveAll(this);
	}
	IAvaMediaModule::Get().GetOnAvaMediaSyncPackageModified().RemoveAll(this);
	
#if WITH_EDITOR
	UAvalancheMediaSettings::GetMutable().OnSettingChanged().RemoveAll(this);
#endif
}

TSharedPtr<FAvalancheManagedInstance> FAvalancheManagedInstanceCache::GetOrLoadAvalancheInstance(const FSoftObjectPath& InAssetPath)
{
	FinishPendingActions();

	if (InAssetPath.IsNull())
	{
		return nullptr;
	}
	
	OrderQueue.Remove(InAssetPath); // Removes preserves the order. O(n)
	OrderQueue.Add(InAssetPath); // Most recent is at the end of the array.

	if (const TSharedPtr<FAvalancheManagedInstance>* ExistingEntry = AvalancheInstances.Find(InAssetPath))
	{
		if (ExistingEntry->IsValid())
		{
			return *ExistingEntry;
		}
	}

	TSharedPtr<FAvalancheManagedInstance> NewEntry;

	const FString PackageName = InAssetPath.GetLongPackageName();
	if (FAvaMediaPlaybackUtils::IsMapAsset(PackageName))
	{
		NewEntry = MakeShared<FAvalancheManagedInstanceLevel>(this, InAssetPath);
	}
	else
	{
		NewEntry = MakeShared<FAvalancheManagedInstanceBlueprint>(this, InAssetPath);
	}

	if (NewEntry.IsValid() && NewEntry->IsValid())
	{
		AvalancheInstances.Add(InAssetPath, NewEntry);
		TrimCache();
	}

	return NewEntry;
}

void FAvalancheManagedInstanceCache::InvalidateNoDelete(const FSoftObjectPath& InAssetPath)
{
	if (AvalancheInstances.Contains(InAssetPath) && !PendingInvalidatedPaths.Contains(InAssetPath))
	{
		PendingInvalidatedPaths.Add(InAssetPath);
		OnEntryInvalidated.Broadcast(InAssetPath);
	}
}

void FAvalancheManagedInstanceCache::Invalidate(const FSoftObjectPath& InAssetPath)
{
	// Ensure no pending actions. For instance, current path could already be pending
	// and we don't want the events to be fired multiple time.
	FinishPendingActions();

	if (AvalancheInstances.Contains(InAssetPath))
	{
		RemoveEntry(InAssetPath);
		OnEntryInvalidated.Broadcast(InAssetPath);
	}
}

int32 FAvalancheManagedInstanceCache::GetMaximumCacheSize() const
{
	return UAvalancheMediaSettings::Get().ManagedAvalancheInstanceCacheMaximumSize;
}

void FAvalancheManagedInstanceCache::Flush(const FSoftObjectPath& InAssetPath)
{
	if (const TSharedPtr<FAvalancheManagedInstance>* ExistingEntry = AvalancheInstances.Find(InAssetPath))
	{
		if (ExistingEntry->GetSharedReferenceCount() <= 1)
		{
			RemoveEntry(InAssetPath);
		}
	}
}

void FAvalancheManagedInstanceCache::Flush()
{
	RemoveEntries([](const FSoftObjectPath& InAssetPath, const TSharedPtr<FAvalancheManagedInstance>& InManagedAvalancheBlueprint)
	{
		return (InManagedAvalancheBlueprint.GetSharedReferenceCount() <= 1) ? true : false;
	}, false);
}

void FAvalancheManagedInstanceCache::TrimCache()
{
	const int32 MaximumCacheSize = GetMaximumCacheSize(); 
	if (MaximumCacheSize > 0)
	{
		while (OrderQueue.Num() > MaximumCacheSize)
		{
			// LRU: oldest is at the start of the array.
			AvalancheInstances.Remove(OrderQueue[0]);
			OrderQueue.RemoveAt(0);
		}
	}
}

void FAvalancheManagedInstanceCache::FinishPendingActions()
{
	RemovePendingInvalidatedPaths();
}

void FAvalancheManagedInstanceCache::RemovePendingInvalidatedPaths()
{
	for (const FSoftObjectPath& InvalidatedPath : PendingInvalidatedPaths)
	{
		RemoveEntry(InvalidatedPath);
	}
	PendingInvalidatedPaths.Reset();
}

void FAvalancheManagedInstanceCache::RemoveEntry(const FSoftObjectPath& InAssetPath)
{
	OrderQueue.Remove(InAssetPath);
	AvalancheInstances.Remove(InAssetPath);
}

void FAvalancheManagedInstanceCache::RemoveEntries(TFunctionRef<bool(const FSoftObjectPath&, const TSharedPtr<FAvalancheManagedInstance>&)> InRemovePredicate, bool bInNotify)
{
	TArray<FSoftObjectPath> AssetsToNotify;
	
	for (TMap<FSoftObjectPath, TSharedPtr<FAvalancheManagedInstance>>::TIterator EntryIt = AvalancheInstances.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (InRemovePredicate(EntryIt.Key(), EntryIt.Value()))
		{
			if (bInNotify)
			{
				AssetsToNotify.Add(EntryIt.Key());
			}
			OrderQueue.Remove(EntryIt.Key());
			EntryIt.RemoveCurrent();
		}
	}

	// Notify outside of the clean up loop since it is very likely the result of this
	// will be a reload of the asset that got invalidated.
	for (const FSoftObjectPath& AssetPath : AssetsToNotify)
	{
		OnEntryInvalidated.Broadcast(AssetPath);
	}
}

void FAvalancheManagedInstanceCache::OnPackageSaved(const FString& InPackageFileName, UPackage* InPackage, FObjectPostSaveContext InObjectSaveContext)
{
	if (InObjectSaveContext.IsProceduralSave())
	{
		return;
	}

	OnPackageModified(InPackage->GetFName());
}

void FAvalancheManagedInstanceCache::OnAvaSyncPackageModified(IAvaMediaSyncProvider* InAvaMediaSyncProvider, const FName& InPackageName)
{
	UE_LOG(LogAvaMedia, Verbose,
		TEXT("A sync operation has touched the package \"%s\" on disk. ManagedAvalancheBlueprintCache notified."),
		*InPackageName.ToString());

	OnPackageModified(InPackageName);
}

void FAvalancheManagedInstanceCache::OnAssetRemoved(const FAssetData& InAssetData)
{
	// Invalidate the internal cache for the given package.
	OnPackageModified(InAssetData.PackageName);
}

void FAvalancheManagedInstanceCache::OnPackageModified(const FName& InPackageName)
{
	// Invalidate corresponding assets from that package.
	RemoveEntries([InPackageName](const FSoftObjectPath& InAssetPath, const TSharedPtr<FAvalancheManagedInstance>&)
		{
			if (InAssetPath.GetLongPackageFName() == InPackageName)
			{
				UE_LOG(LogAvaMedia, Log,
					TEXT("Managed Avalanche Blueprint Cache: Package \"%s\" being touched caused asset \"%s\" to be invalidated."),
					*InPackageName.ToString(), *InAssetPath.ToString());
				return true;
			}
			return false;
		}, true);	// Notify of asset being invalidated to trigger a UI refresh.
}

void FAvalancheManagedInstanceCache::OnSettingChanged(UObject* , struct FPropertyChangedEvent&)
{
	TrimCache();
}