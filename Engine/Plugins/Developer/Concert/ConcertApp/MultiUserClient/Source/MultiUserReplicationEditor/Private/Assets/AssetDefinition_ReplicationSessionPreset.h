// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "AssetDefinition_ReplicationSessionPreset.generated.h"

/**
 * 
 */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UAssetDefinition_ReplicationSessionPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()
public:
	
	//~ Begin UAssetDefinition Interface
	virtual FText GetAssetDisplayName() const override; 
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override; 
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	//~ End UAssetDefinition Interface
};
