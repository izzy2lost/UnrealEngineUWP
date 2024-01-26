// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedRenamerObjectProviderInterface.h"
#include "AdvancedRenamerAssetProvider.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AdvancedRenamerObjectAssetProvider.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable)
class ADVANCEDRENAMER_API UAdvancedRenamerAssetProvider : public UObject, public IAdvancedRenamerObjectProvider
#if CPP
	, public FAdvancedRenamerAssetProvider
#endif
{
	GENERATED_BODY()

public:

	UAdvancedRenamerAssetProvider()
	{
	}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Set Asset List"))
	void BP_SetAssetList(const TArray<UObject*>& InAssetList);
	virtual void BP_SetAssetList_Implementation(const TArray<UObject*>& InAssetList);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Add Asset List"))
	void BP_AddAssetList(const TArray<UObject*>& InAssetList);
	virtual void BP_AddAssetList_Implementation(const TArray<UObject*>& InAssetList);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Add Asset Data"))
	void BP_AddAssetData(UObject* InObject);
	virtual void BP_AddAssetData_Implementation(UObject* InObject);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Advanced Rename Panel", meta = (DisplayName = "Get Asset"))
	UObject* BP_GetAsset(int32 Index) const;
	virtual UObject* BP_GetAsset_Implementation(int32 Index) const;

	virtual int32 Num() const override;
	virtual bool IsValidIndex(int32 Index) const override;
	virtual uint32 GetHash(int32 Index) const override;
	virtual FString GetOriginalName(int32 Index) const override;
	virtual bool RemoveIndex(int32 Index) override;
	virtual bool CanRename(int32 Index) const override;
	virtual bool ExecuteRename(int32 Index, const FString & NewName) override;
};
