// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Containers/ContainersFwd.h"
#include "Material/DynamicMaterialInstance.h"

class FExtender;
class FMenuBuilder;
class UDMTextureSet;
class UDynamicMaterialInstance;
class UDynamicMaterialModel;
struct FAssetData;

class FDMContentBrowserIntegration
{
public:
	static void Integrate();

	static void Disintegrate();

	static void UpdateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets, bool bInReplace);

protected:
	static FDelegateHandle TextureSetPopulateHandle;
	static FDelegateHandle ContentBrowserAssetHandle;

	static void ExtendMenu(FMenuBuilder& InMenuBuilder, const TArray<FAssetData>& InSelectedAssets);

	static void CreateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets);

	static void OnCreateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted, FString InPath);

	static void OnUpdateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted, bool bInReplace);

	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& InSelectedAssets);

	static void CreateDynamic(TArray<FAssetData> InSelectedAssets);

	static void CreateModelDynamic(UDynamicMaterialModel* InModel);

	static void CreateInstanceDynamic(UDynamicMaterialInstance* InInstance);
};
