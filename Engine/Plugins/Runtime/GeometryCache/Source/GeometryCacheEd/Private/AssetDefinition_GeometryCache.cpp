// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_GeometryCache.h"

#include "EditorFramework/AssetImportData.h"
#include "GeometryCache.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText UAssetDefinition_GeometryCache::GetAssetDisplayName() const
{
	return LOCTEXT("AssetTypeActions_GeometryCache", "GeometryCache");
}

FLinearColor UAssetDefinition_GeometryCache::GetAssetColor() const
{
	return FColor(0, 255, 255);
}

TSoftClassPtr<UObject> UAssetDefinition_GeometryCache::GetAssetClass() const
{
	return UGeometryCache::StaticClass();
}

bool UAssetDefinition_GeometryCache::CanImport() const
{
	return true;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_GeometryCache::GetAssetCategories() const
{
	static const auto Categories = {EAssetCategoryPaths::Animation};
	return Categories;
}

#undef LOCTEXT_NAMESPACE
