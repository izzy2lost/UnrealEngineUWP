// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorMenuUtils.h"

#include "PCGLevelToAsset.h"

#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "PCGEditorMenuUtils"

namespace PCGEditorMenuUtils
{
	FToolMenuSection& CreatePCGSection(UToolMenu* Menu)
	{
		const FName LevelSectionName = TEXT("PCG");
		FToolMenuSection* SectionPtr = Menu->FindSection(LevelSectionName);
		if (!SectionPtr)
		{
			SectionPtr = &(Menu->AddSection(LevelSectionName, LOCTEXT("PCGSectionLabel", "Procedural (PCG)")));
		}

		FToolMenuSection& Section = *SectionPtr;
		return Section;
	}

	void CreateOrUpdatePCGAssetFromMenu(UToolMenu* Menu, TArray<FAssetData>& InWorldAssets)
	{
		TArray<FAssetData> TempWorldAssets;
		for (const FAssetData& Asset : InWorldAssets)
		{
			if (Asset.IsInstanceOf<UWorld>())
			{
				TempWorldAssets.Add(Asset);
			}
		}

		if (TempWorldAssets.IsEmpty())
		{
			return;
		}

		FToolMenuSection& Section = CreatePCGSection(Menu);

		FToolUIAction UIAction;
		UIAction.ExecuteAction.BindLambda([WorldAssets = MoveTemp(TempWorldAssets)](const FToolMenuContext& MenuContext)
		{
			FScopedSlowTask SlowTask(0.0f, LOCTEXT("CreateOrUpdatePCGAssetsInProgress", "Creating PCG Assets..."));
			UPCGLevelToAsset::CreateOrUpdatePCGAssets(WorldAssets);
		});

		Section.AddMenuEntry(
			"CreateOrUpdatePCGAssetFromMenu",
			LOCTEXT("CreateOrUpdatePCGAssetFromMenu", "Create or Update PCG Asset from Level(s)"),
			TAttribute<FText>(),
			FSlateIcon(),
			UIAction);
	}
}

#undef LOCTEXT_NAMESPACE