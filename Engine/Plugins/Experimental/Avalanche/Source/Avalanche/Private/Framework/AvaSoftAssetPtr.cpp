// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/AvaSoftAssetPtr.h"
#include "AvaBlueprint.h"

EAvalancheAssetType FAvaSoftAssetPath::GetAssetTypeFromClass(const FSoftClassPath& InAssetClassPath, bool bInLoadIfUnknown)
{
	static const FSoftClassPath AvalancheClassPath(UAvalancheBlueprint::StaticClass());
	static const FSoftClassPath WorldClassPath(UWorld::StaticClass());

	// Try comparing the asset paths directly.
	// That is probably the fastest way, but can fail for derived types.
	if (InAssetClassPath == AvalancheClassPath)
	{
		return EAvalancheAssetType::Blueprint;
	}
	if (InAssetClassPath == WorldClassPath)
	{
		return EAvalancheAssetType::World;
	}

	// Try to load the asset class and compare possible derived types.
	if (InAssetClassPath.IsValid())
	{
		const UClass* AssetClass = InAssetClassPath.ResolveClass();
		if (AssetClass->IsChildOf(UAvalancheBlueprint::StaticClass()))
		{
			return EAvalancheAssetType::Blueprint;
		}
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
	
		if (Cast<UAvalancheBlueprint>(LoadedSourceAsset))
		{
			return EAvalancheAssetType::Blueprint;
		}
	
		if (Cast<UWorld>(LoadedSourceAsset))
		{
			return EAvalancheAssetType::World;
		}
	}
	
	return EAvalancheAssetType::Unknown;
}