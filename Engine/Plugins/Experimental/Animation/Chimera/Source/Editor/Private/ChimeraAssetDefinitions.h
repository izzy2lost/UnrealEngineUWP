// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Chimera/ChimeraAsset.h"
#include "ChimeraAssetDefinitions.generated.h"

namespace UE::Chimera
{
	FLinearColor GetAssetColor();
	TConstArrayView<FAssetCategoryPath> GetAssetCategories();
	UThumbnailInfo* LoadThumbnailInfo(const FAssetData& InAssetData);
} // namespace UE::Chimera

UCLASS()
class UAssetDefinition_ChimeraAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FLinearColor GetAssetColor() const override { return UE::Chimera::GetAssetColor(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override { return UE::Chimera::GetAssetCategories(); }
	virtual UThumbnailInfo* LoadThumbnailInfo(const FAssetData& InAssetData) const override { return UE::Chimera::LoadThumbnailInfo(InAssetData); }

	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("ChimeraAssetDefinition", "DisplayName_ChimeraAsset", "Chimera Asset"); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UChimeraAsset::StaticClass(); }
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

