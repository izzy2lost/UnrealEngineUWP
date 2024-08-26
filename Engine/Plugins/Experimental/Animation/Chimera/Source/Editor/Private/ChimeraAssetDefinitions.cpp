// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChimeraAssetDefinitions.h"
#include "ChimeraAssetEditor.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

namespace UE::Chimera
{

FLinearColor GetAssetColor()
{
	static const FLinearColor AssetColor(FColor(6, 9, 53));
	return AssetColor;
}

TConstArrayView<FAssetCategoryPath> GetAssetCategories()
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, NSLOCTEXT("ChimeraAssetDefinition", "ChimeraAssetDefinitionMenu", "Chimera")) };
	return Categories;
}

UThumbnailInfo* LoadThumbnailInfo(const FAssetData & InAssetData)
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), USceneThumbnailInfo::StaticClass());
}

} // namespace UE::Chimera

EAssetCommandResult UAssetDefinition_ChimeraAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::Chimera;

	TArray<UChimeraAsset*> Objects = OpenArgs.LoadObjects<UChimeraAsset>();
	const EToolkitMode::Type Mode = OpenArgs.ToolkitHost.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UChimeraAsset* ChimeraAsset : Objects)
	{
		if (ChimeraAsset)
		{
			TSharedRef<FChimeraAssetEditor> NewEditor(new FChimeraAssetEditor());
			NewEditor->InitAssetEditor(Mode, OpenArgs.ToolkitHost, ChimeraAsset);
		}
	}
	
	return EAssetCommandResult::Handled;
}