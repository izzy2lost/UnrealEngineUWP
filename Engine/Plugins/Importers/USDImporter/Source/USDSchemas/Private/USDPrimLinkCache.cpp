// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDPrimLinkCache.h"

#include "USDLog.h"
#include "UsdWrappers/SdfPath.h"

struct UUsdPrimLinkCache::UUsdPrimLinkCacheImpl
{
	UUsdPrimLinkCacheImpl() = default;
	UUsdPrimLinkCacheImpl(const UUsdPrimLinkCacheImpl& Other)
		: UUsdPrimLinkCacheImpl()
	{
		FReadScopeLock OtherScopedPrimPathToAssetsLock(Other.PrimPathToAssetsLock);
		FWriteScopeLock ThisScopedPrimPathToAssetsLock(PrimPathToAssetsLock);

		PrimPathToAssets = Other.PrimPathToAssets;
		AssetToPrimPaths = Other.AssetToPrimPaths;
	}

	// Information we may have about a subset of prims
	TMap<UE::FSdfPath, TArray<TWeakObjectPtr<UObject>>> PrimPathToAssets;
	TMap<TWeakObjectPtr<UObject>, TArray<UE::FSdfPath>> AssetToPrimPaths;
	mutable FRWLock PrimPathToAssetsLock;
};

UUsdPrimLinkCache::UUsdPrimLinkCache()
{
	Impl = MakeUnique<UUsdPrimLinkCache::UUsdPrimLinkCacheImpl>();
}

// Boilerplate to support Pimpl in an UObject
// See UniquePtr.h, TDefaultDelete
UUsdPrimLinkCache::UUsdPrimLinkCache(FVTableHelper& Helper)
	: Super(Helper)
{
}

UUsdPrimLinkCache::~UUsdPrimLinkCache()
{
}

void UUsdPrimLinkCache::Serialize(FArchive& Ar)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UUsdPrimLinkCache::Serialize);

	if (UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get())
	{
		FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
		Ar << ImplPtr->PrimPathToAssets;
		Ar << ImplPtr->AssetToPrimPaths;
	}
}

bool UUsdPrimLinkCache::ContainsInfoAboutPrim(const UE::FSdfPath& Path) const
{
	if (UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
		return ImplPtr->PrimPathToAssets.Contains(Path);
	}

	return false;
}

TSet<UE::FSdfPath> UUsdPrimLinkCache::GetKnownPrims() const
{
	TSet<UE::FSdfPath> Result;

	if (UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
		ImplPtr->PrimPathToAssets.GetKeys(Result);
		return Result;
	}

	return Result;
}

void UUsdPrimLinkCache::LinkAssetToPrim(const UE::FSdfPath& Path, UObject* Asset)
{
	UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}

	bool bAlwaysMarkDirty = false;
	Modify(bAlwaysMarkDirty);

	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Linking asset '%s' to prim '%s'"), *Asset->GetPathName(), *Path.GetString());

	ImplPtr->PrimPathToAssets.FindOrAdd(Path).AddUnique(Asset);
	ImplPtr->AssetToPrimPaths.FindOrAdd(Asset).AddUnique(Path);
}

void UUsdPrimLinkCache::UnlinkAssetFromPrim(const UE::FSdfPath& Path, UObject* Asset)
{
	UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}

	bool bAlwaysMarkDirty = false;
	Modify(bAlwaysMarkDirty);

	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Unlinking asset '%s' to prim '%s'"), *Asset->GetPathName(), *Path.GetString());

	if (TArray<TWeakObjectPtr<UObject>>* FoundAssetsForPrim = ImplPtr->PrimPathToAssets.Find(Path))
	{
		FoundAssetsForPrim->Remove(Asset);
	}
	if (TArray<UE::FSdfPath>* FoundPrimPathsForAsset = ImplPtr->AssetToPrimPaths.Find(Asset))
	{
		FoundPrimPathsForAsset->Remove(Path);
	}
}

TArray<TWeakObjectPtr<UObject>> UUsdPrimLinkCache::RemoveAllAssetPrimLinks(const UE::FSdfPath& Path)
{
	UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}

	bool bAlwaysMarkDirty = false;
	Modify(bAlwaysMarkDirty);

	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Removing asset prim links for path '%s'"), *Path.GetString());

	TArray<TWeakObjectPtr<UObject>> Assets;
	ImplPtr->PrimPathToAssets.RemoveAndCopyValue(Path, Assets);

	for (const TWeakObjectPtr<UObject>& Asset : Assets)
	{
		if (TArray<UE::FSdfPath>* PrimPaths = ImplPtr->AssetToPrimPaths.Find(Asset))
		{
			PrimPaths->Remove(Path);
		}
	}

	return Assets;
}

TArray<UE::FSdfPath> UUsdPrimLinkCache::RemoveAllAssetPrimLinks(const UObject* Asset)
{
	UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}

	bool bAlwaysMarkDirty = false;
	Modify(bAlwaysMarkDirty);

	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Removing asset prim links for asset '%s'"), *Asset->GetPathName());

	TArray<UE::FSdfPath> PrimPaths;
	ImplPtr->AssetToPrimPaths.RemoveAndCopyValue(const_cast<UObject*>(Asset), PrimPaths);

	for (const UE::FSdfPath& Path : PrimPaths)
	{
		if (TArray<TWeakObjectPtr<UObject>>* Assets = ImplPtr->PrimPathToAssets.Find(Path))
		{
			Assets->Remove(const_cast<UObject*>(Asset));
		}
	}

	return PrimPaths;
}

void UUsdPrimLinkCache::RemoveAllAssetPrimLinks()
{
	UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}

	bool bAlwaysMarkDirty = false;
	Modify(bAlwaysMarkDirty);

	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Removing all asset prim links"));

	ImplPtr->PrimPathToAssets.Empty();
	ImplPtr->AssetToPrimPaths.Empty();
}

TArray<TWeakObjectPtr<UObject>> UUsdPrimLinkCache::GetAllAssetsForPrim(const UE::FSdfPath& Path) const
{
	UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	if (const TArray<TWeakObjectPtr<UObject>>* FoundAssets = ImplPtr->PrimPathToAssets.Find(Path))
	{
		return *FoundAssets;
	}

	return {};
}

TArray<UE::FSdfPath> UUsdPrimLinkCache::GetPrimsForAsset(const UObject* Asset) const
{
	if (!Asset)
	{
		return {};
	}

	UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	if (const TArray<UE::FSdfPath>* FoundPrims = ImplPtr->AssetToPrimPaths.Find(Asset))
	{
		return *FoundPrims;
	}

	return {};
}

TMap<UE::FSdfPath, TArray<TWeakObjectPtr<UObject>>> UUsdPrimLinkCache::GetAllAssetPrimLinks() const
{
	UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}

	return ImplPtr->PrimPathToAssets;
}

void UUsdPrimLinkCache::Clear()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AUsdStageActor::UnloadUsdStage);

	if (UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get())
	{
		bool bAlwaysMarkDirty = false;
		Modify(bAlwaysMarkDirty);

		TRACE_CPUPROFILER_EVENT_SCOPE(PrimPathToAssetsEmpty);
		FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
		ImplPtr->PrimPathToAssets.Empty();
		ImplPtr->AssetToPrimPaths.Empty();
	}
}

bool UUsdPrimLinkCache::IsEmpty()
{
	if (UUsdPrimLinkCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
		return ImplPtr->PrimPathToAssets.IsEmpty();
	}

	return true;
}
