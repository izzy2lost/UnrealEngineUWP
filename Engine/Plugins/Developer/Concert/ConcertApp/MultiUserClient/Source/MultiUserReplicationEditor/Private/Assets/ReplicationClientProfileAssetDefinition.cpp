// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClientProfileAssetDefinition.h"

#include "IMultiUserReplicationEditorModule.h"
#include "MultiUserReplicationClientProfileAsset.h"

#define LOCTEXT_NAMESPACE "UReplicationClientProfileAssetDefinition"

FText UReplicationClientProfileAssetDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("AssetName", "Replication Client Profile");
}

FLinearColor UReplicationClientProfileAssetDefinition::GetAssetColor() const
{
	return FLinearColor(1.f, 0.8f, 0.2f);
}

TSoftClassPtr<UObject> UReplicationClientProfileAssetDefinition::GetAssetClass() const
{
	return UMultiUserReplicationClientProfileAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UReplicationClientProfileAssetDefinition::GetAssetCategories() const
{
	static const TArray<FAssetCategoryPath> Categories = { UE::MultiUserReplicationEditor::IMultiUserReplicationEditorModule::Get().GetMultiUserReplicationCategory() };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
