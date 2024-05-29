// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Containers/ContainersFwd.h"

class FMenuBuilder;
class UDMTextureSet;
struct FAssetData;

class FDMContentBrowserIntegration
{
public:
	static void Integrate();

	static void Disintegrate();

	static void UpdateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets, bool bInReplace);

protected:
	static void ExtendMenu(FMenuBuilder& InMenuBuilder, const TArray<FAssetData>& InSelectedAssets);

	static void CreateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets);

	static void OnCreateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted, FString InPath);

	static void OnUpdateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted, bool bInReplace);

	static FDelegateHandle PopulateHandle;
};
