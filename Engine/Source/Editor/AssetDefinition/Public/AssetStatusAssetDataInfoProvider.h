// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionStatusBase.h"
#include "AssetRegistry/AssetData.h"

class ASSETDEFINITION_API FAssetStatusAssetDataInfoProvider : public IAssetStatusInfoProvider
{
public:
	FAssetStatusAssetDataInfoProvider(FAssetData InAssetData)
		: AssetData(InAssetData)
	{}

	virtual UPackage* FindPackage() const override;

	virtual FString TryGetFilename() const override;

private:
	FAssetData AssetData;
};
