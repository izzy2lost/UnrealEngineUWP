// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointerFwd.h"

class FExtender;
class UDMTextureSet;
struct FAssetData;

class FDMTextureSetContentBrowserIntegration
{
public:
	static void Integrate();

	static void Disintegrate();

protected:
	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& InSelectedAssets);

	static void CreateTextureSet(TArray<FAssetData> InSelectedAssets);

	static void OnCreateTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted, FString InPath);

	static void CreateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets);

	static void OnCreateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted, FString InPath);

	static void UpdateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets, bool bInReplace);

	static void OnUpdateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted, bool bInReplace);

	static FDelegateHandle ExtenderDelegateHandle;
};
