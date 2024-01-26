// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedRenamerProviderInterface.h"
#include "AssetRegistry/AssetData.h"
#include "Containers/Array.h"

struct FAssetData;

class ADVANCEDRENAMER_API FAdvancedRenamerAssetProvider : public IAdvancedRenamerProvider
{
public:
	FAdvancedRenamerAssetProvider();
	virtual ~FAdvancedRenamerAssetProvider() override;

	void SetAssetList(const TArray<FAssetData>& InAssetList);
	void AddAssetList(const TArray<FAssetData>& InAssetList);
	void AddAssetData(const FAssetData& InAsset);
	UObject* GetAsset(int32 Index) const;

	virtual int32 Num() const override;
	virtual bool IsValidIndex(int32 Index) const override;
	virtual uint32 GetHash(int32 Index) const override;;
	virtual FString GetOriginalName(int32 Index) const override;
	virtual bool RemoveIndex(int32 Index) override;
	virtual bool CanRename(int32 Index) const override;
	virtual bool ExecuteRename(int32 Index, const FString& NewName) override;

protected:
	TArray<FAssetData> AssetList;
};
