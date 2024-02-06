// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/AvaSoftAssetPtr.h"
#include "Engine/World.h"

EAvalancheAssetType FAvaSoftAssetPath::GetAssetTypeFromClass(const FSoftClassPath& InAssetClassPath, bool bInLoadIfUnknown)
{
	static const FSoftClassPath WorldClassPath(UWorld::StaticClass());

	if (InAssetClassPath == WorldClassPath)
	{
		return EAvalancheAssetType::World;
	}

	// Try to load the asset class and compare possible derived types.
	if (InAssetClassPath.IsValid())
	{
		const UClass* AssetClass = InAssetClassPath.ResolveClass();
		if (AssetClass->IsChildOf(UWorld::StaticClass()))
		{
			return EAvalancheAssetType::World;
		}
	}
	return EAvalancheAssetType::Unknown;
}

EAvalancheAssetType FAvaSoftAssetPtr::GetAssetType(bool bInLoadIfUnknown) const
{
	const EAvalancheAssetType AssetTypeFromClass = FAvaSoftAssetPath::GetAssetTypeFromClass(AssetClassPath, bInLoadIfUnknown);
	if (AssetTypeFromClass != EAvalancheAssetType::Unknown)
	{
		return AssetTypeFromClass;
	}

	if (bInLoadIfUnknown)
	{
		// Todo: problem with loading the asset sync here.
		// Todo: Investigate LoadPackageAsync.
		// This will cause a hitch. For level asset, we want to avoid that.
		UObject* LoadedSourceAsset;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FAvaSoftAssetPtr::LoadSourceAsset);
			const FSoftObjectPtr SourceAvalancheAsset(AssetPtr.ToSoftObjectPath());
			LoadedSourceAsset = SourceAvalancheAsset.LoadSynchronous();
		}

		if (Cast<UWorld>(LoadedSourceAsset))
		{
			return EAvalancheAssetType::World;
		}
	}

	return EAvalancheAssetType::Unknown;
}