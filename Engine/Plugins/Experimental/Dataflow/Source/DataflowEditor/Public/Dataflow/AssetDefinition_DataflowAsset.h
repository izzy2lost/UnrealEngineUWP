// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinition_DataflowAsset.generated.h"


namespace DataflowAssetDefinitionHelpers
{
	// Return true if we should proceed, false if we should re-open the dialog
	DATAFLOWEDITOR_API bool CreateNewDataflowAsset(const UObject* Asset, UObject*& OutDataflowAsset);

	// Return true if we should proceed, false if we should re-open the dialog
	DATAFLOWEDITOR_API bool OpenDataflowAsset(const UObject* Asset, UObject*& OutDataflowAsset);

	// Return true if we should proceed, false if we should re-open the dialog
	DATAFLOWEDITOR_API bool NewOrOpenDialog(const UObject* Asset, UObject*& OutDataflowAsset);

	// Create a new UDataflow if one doesn't already exist for the Cloth Asset
	DATAFLOWEDITOR_API UObject* NewOrOpenDataflowAsset(const UObject* Asset);
}



UCLASS()
class UAssetDefinition_DataflowAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

private:

	virtual FText GetAssetDisplayName() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual UThumbnailInfo* LoadThumbnailInfo(const FAssetData& InAssetData) const override;

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

