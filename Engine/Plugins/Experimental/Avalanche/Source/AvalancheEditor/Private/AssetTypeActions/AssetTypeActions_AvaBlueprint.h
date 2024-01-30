// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"

/** We override Base, and not Blueprint, to not allow creation of Child Classes. */
class FAssetTypeActions_AvaBlueprint : public FAssetTypeActions_Base
{
public:
	//~ Begin IAssetTypeActions
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override { return FColor(44, 89, 180); }
	virtual UClass* GetSupportedClass() const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Blueprint; }
	virtual void PerformAssetDiff(UObject* Asset1, UObject* Asset2, const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const override;
	//~ End IAssetTypeActions
};
