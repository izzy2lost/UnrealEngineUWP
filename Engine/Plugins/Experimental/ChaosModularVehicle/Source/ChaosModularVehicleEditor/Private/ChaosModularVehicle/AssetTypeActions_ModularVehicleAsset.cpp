// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/AssetTypeActions_ModularVehicleAsset.h"

#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "ChaosModularVehicle/ModularVehicleAsset.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_ModularVehicleAsset::GetSupportedClass() const
{
	return UModularVehicleAsset::StaticClass();
}

UThumbnailInfo* FAssetTypeActions_ModularVehicleAsset::GetThumbnailInfo(UObject* Asset) const
{
	UModularVehicleAsset * ModularVehicleAsset = CastChecked<UModularVehicleAsset>(Asset);
	UThumbnailInfo* ThumbnailInfo = ModularVehicleAsset->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(ModularVehicleAsset, NAME_None, RF_Transactional);
		ModularVehicleAsset->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_ModularVehicleAsset::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	FAssetTypeActions_Base::GetActions(InObjects, Section);
}


#undef LOCTEXT_NAMESPACE
