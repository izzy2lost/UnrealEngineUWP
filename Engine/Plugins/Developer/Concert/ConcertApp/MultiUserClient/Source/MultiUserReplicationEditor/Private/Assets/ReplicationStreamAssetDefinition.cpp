// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationStreamAssetDefinition.h"

#include "StreamEditor/ReplicationStreamAssetEditor.h"
#include "IMultiUserReplicationEditorModule.h"
#include "MultiUserReplicationStreamAsset.h"

#define LOCTEXT_NAMESPACE "UReplicationStreamAssetDefinition"

FText UReplicationStreamAssetDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("AssetName", "Replication Stream");
}

FLinearColor UReplicationStreamAssetDefinition::GetAssetColor() const
{
	return FLinearColor(4.f, 0.4f, 0.65f);
}

TSoftClassPtr<UObject> UReplicationStreamAssetDefinition::GetAssetClass() const
{
	return UMultiUserReplicationStreamAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UReplicationStreamAssetDefinition::GetAssetCategories() const
{
	static const TArray<FAssetCategoryPath> Categories = { UE::MultiUserReplicationEditor::IMultiUserReplicationEditorModule::Get().GetMultiUserReplicationCategory() };
	return Categories;
}

EAssetCommandResult UReplicationStreamAssetDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return EAssetCommandResult::Handled;
	}

	for (UMultiUserReplicationStreamAsset* ClientProfile : OpenArgs.LoadObjects<UMultiUserReplicationStreamAsset>())
	{
		UReplicationStreamAssetEditor* AssetEditor = NewObject<UReplicationStreamAssetEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
		if (!AssetEditor)
		{
			continue;
		}

		AssetEditor->SetObjectToEdit(ClientProfile);
		AssetEditor->Initialize();
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
