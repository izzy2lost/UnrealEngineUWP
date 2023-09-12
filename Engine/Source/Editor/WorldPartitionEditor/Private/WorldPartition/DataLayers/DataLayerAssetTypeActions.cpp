// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayerAssetTypeActions.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

FText FDataLayerAssetTypeActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DataLayerAsset", "DataLayer Asset");
}

FColor FDataLayerAssetTypeActions::GetTypeColor() const
{
	return FColor(0, 200, 200);
}

UClass* FDataLayerAssetTypeActions::GetSupportedClass() const
{
	return UDataLayerAsset::StaticClass();
}

uint32 FDataLayerAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::World;
}

bool FDataLayerAssetTypeActions::CanLocalize() const
{
	return false;
}
