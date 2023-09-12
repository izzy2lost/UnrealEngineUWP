// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/AssetDefinition_DataflowAsset.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEditorModule.h"
#include "Dataflow/DataflowObject.h"
#include "Math/Color.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"


#define LOCTEXT_NAMESPACE "AssetActions_DataflowAsset"


namespace UE::Dataflow::DataflowAsset
{
	struct FColorScheme
	{
		static inline const FLinearColor Asset = FColor(180, 120, 110);
		static inline const FLinearColor NodeHeader = FColor(180, 120, 110);
		static inline const FLinearColor NodeBody = FColor(18, 12, 11, 127);
	};
}

FText UAssetDefinition_DataflowAsset::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DataflowAsset", "DataflowAsset");
}

TSoftClassPtr<UObject> UAssetDefinition_DataflowAsset::GetAssetClass() const
{
	return UDataflow::StaticClass();
}

FLinearColor UAssetDefinition_DataflowAsset::GetAssetColor() const
{
	return UE::Dataflow::DataflowAsset::FColorScheme::Asset;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_DataflowAsset::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Physics };
	return Categories;
}

UThumbnailInfo* UAssetDefinition_DataflowAsset::LoadThumbnailInfo(const FAssetData& InAsset) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAsset.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_DataflowAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UDataflow*> DataflowObjects = OpenArgs.LoadObjects<UDataflow>();

	// For now the dataflow editor only works on one asset at a time
	ensure(DataflowObjects.Num() == 0 || DataflowObjects.Num() == 1);

	if (DataflowObjects.Num() == 1)
	{
		UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		UDataflowEditor* const AssetEditor = NewObject<UDataflowEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);

		// Validate the asset
		if (UDataflow* const DataflowAsset = CastChecked<UDataflow>(DataflowObjects[0]))
		{
			AssetEditor->Initialize({ DataflowAsset });
			return EAssetCommandResult::Handled;
		}
	}

	return EAssetCommandResult::Unhandled;
}


#undef LOCTEXT_NAMESPACE
