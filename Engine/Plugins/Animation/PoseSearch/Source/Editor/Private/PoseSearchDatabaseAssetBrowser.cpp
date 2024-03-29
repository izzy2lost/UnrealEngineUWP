// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseAssetBrowser.h"

#include "ContentBrowserDataSource.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Widgets/SBoxPanel.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

namespace UE::PoseSearch
{
	
void SPoseSearchDatabaseAssetBrowser::Construct(const FArguments& InArgs, TSharedPtr<FDatabaseViewModel> InViewModel)
{
	DatabaseViewModel = InViewModel;
	
	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SAssignNew(AssetBrowserBox, SBox)
			]
		];

	RefreshView();
}

void SPoseSearchDatabaseAssetBrowser::RefreshView()
{
	// @TODO: Add support for MultiSequence.
	
	FAssetPickerConfig AssetPickerConfig;
	
	AssetPickerConfig.Filter.ClassPaths.Add(UAnimationAsset::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.bRecursiveClasses = true;
	
	AssetPickerConfig.bAddFilterUI = true;
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.bShowPathInColumnView = true;
	AssetPickerConfig.bShowTypeInColumnView = false;
	AssetPickerConfig.InitialThumbnailSize = EThumbnailSize::Small;
	
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SPoseSearchDatabaseAssetBrowser::OnShouldFilterAsset);
	AssetPickerConfig.OnAssetDoubleClicked = FOnAssetSelected::CreateSP(this, &SPoseSearchDatabaseAssetBrowser::OnAssetDoubleClicked);

	// Hide all asset registry columns by default (we only really want the name and path)
	const UObject* AnimSequenceDefaultObject = UAnimSequence::StaticClass()->GetDefaultObject();
	FAssetRegistryTagsContextData TagsContext(AnimSequenceDefaultObject, EAssetRegistryTagsCaller::Uncategorized);
	AnimSequenceDefaultObject->GetAssetRegistryTags(TagsContext);
	for (const TPair<FName, UObject::FAssetRegistryTag>& TagPair : TagsContext.Tags)
	{
		AssetPickerConfig.HiddenColumnNames.Add(TagPair.Key.ToString());
	}

	// Also hide the type column by default (but allow users to enable it, so don't use bShowTypeInColumnView)
	AssetPickerConfig.HiddenColumnNames.Add(TEXT("Class"));
	AssetPickerConfig.HiddenColumnNames.Add(ContentBrowserItemAttributes::ItemDiskSize.ToString());
	AssetPickerConfig.HiddenColumnNames.Add(ContentBrowserItemAttributes::VirtualizedData.ToString());

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	AssetBrowserBox->SetContent(ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig));
}

void SPoseSearchDatabaseAssetBrowser::OnAssetDoubleClicked(const FAssetData& AssetData)
{
	if (!AssetData.GetAsset())
	{
		return;
	}

	UAnimationAsset* NewAnimationAsset = Cast<UAnimationAsset>(AssetData.GetAsset());
	if (!NewAnimationAsset)
	{
		return;
	}

	// Just open asset in persona.
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewAnimationAsset);
}

bool SPoseSearchDatabaseAssetBrowser::OnShouldFilterAsset(const FAssetData& AssetData)
{
	bool bAssetHasCompatibleSkeleton = false;

	if (DatabaseViewModel)
	{
		if (const UPoseSearchDatabase* Database = DatabaseViewModel->GetPoseSearchDatabase())
		{
			if (Database->Schema)
			{
				TArray<FPoseSearchRoledSkeleton> RoledSkeletons = Database->Schema->GetRoledSkeletons();

				for (const FPoseSearchRoledSkeleton& RoledSkeleton : RoledSkeletons)
				{
					if (RoledSkeleton.Skeleton && RoledSkeleton.Skeleton->IsCompatibleForEditor(AssetData))
					{
						// We found a compatible skeleton in the schema.
						bAssetHasCompatibleSkeleton = true;
					}
				}
			}
		}
	}
	
	if (AssetData.GetClass()->IsChildOf(UAnimSequence::StaticClass()) && bAssetHasCompatibleSkeleton)
	{
		return false;
	}

	if (AssetData.GetClass()->IsChildOf(UAnimComposite::StaticClass()) && bAssetHasCompatibleSkeleton)
	{
		return false;
	}

	if (AssetData.GetClass()->IsChildOf(UAnimMontage::StaticClass()) && bAssetHasCompatibleSkeleton)
	{
		return false;
	}
	
	if (AssetData.GetClass()->IsChildOf(UBlendSpace::StaticClass()) && bAssetHasCompatibleSkeleton)
	{
		return false;
	}
	
	return true;
}
	
}
