// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinition_DataflowContext.generated.h"

class UDataflowBaseContent;

namespace DataflowContextDefinitionHelpers
{
	// Return true if we should proceed, false if we should re-open the dialog
	template<class T>
	DATAFLOWEDITOR_API TObjectPtr<T> CreateNewDataflowContent(const TObjectPtr<UObject>& ContentOwner);
}



UCLASS()
class UAssetDefinition_DataflowContext : public UAssetDefinitionDefault
{
	GENERATED_BODY()

private:

	virtual FText GetAssetDisplayName() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual UThumbnailInfo* LoadThumbnailInfo(const FAssetData& InAssetData) const override;
};

