// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionEditorUtils.h"
#include "AvaTransitionTree.h"
#include "AvaTransitionTreeEditorData.h"
#include "Behavior/IAvaTransitionBehavior.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ISinglePropertyView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "AvaTransitionEditorUtils"

namespace UE::AvaTransitionEditor
{

TSharedPtr<SWidget> CreateTransitionLayerPicker(UAvaTransitionTreeEditorData* InEditorData)
{
	if (!InEditorData)
	{
		return nullptr;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FSinglePropertyParams SinglePropertyParams;
	SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Hidden;

	TSharedPtr<ISinglePropertyView> PropertyView = PropertyEditorModule.CreateSingleProperty(InEditorData
		, UAvaTransitionTreeEditorData::GetTransitionLayerPropertyName()
		, SinglePropertyParams);

	if (PropertyView.IsValid())
	{
		return SNew(SBox)
			.Padding(0.f, -6.f)
			.HeightOverride(28.f)
			.MaxDesiredWidth(200.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				// Scale down the widget so as to fit the Toolbar without making the Toolbar Bigger
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFitY)
				.StretchDirection(EStretchDirection::DownOnly)
				.HAlign(HAlign_Fill)
				[
					PropertyView.ToSharedRef()
				]
			];
	}
	return nullptr;
}

bool PickTransitionTreeAsset(const FText& InDialogTitle, UAvaTransitionTree*& OutTransitionTree)
{
	FOpenAssetDialogConfig SelectAssetConfig;
	SelectAssetConfig.DialogTitleOverride = InDialogTitle;
	SelectAssetConfig.bAllowMultipleSelection = false;
	SelectAssetConfig.DefaultPath = TEXT("/Game");
	SelectAssetConfig.AssetClassNames.Add(UAvaTransitionTree::StaticClass()->GetClassPathName());

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<FAssetData> AssetData = ContentBrowserModule.Get().CreateModalOpenAssetDialog(SelectAssetConfig);
	if (AssetData.IsEmpty())
	{
		// return false as user selected no assets
		return false;
	}

	OutTransitionTree = Cast<UAvaTransitionTree>(AssetData[0].GetAsset());
	return true;
}

void ToggleTransitionTreeEnabled(TWeakObjectPtr<UAvaTransitionTree> InTransitionTreeWeak)
{
	if (UAvaTransitionTree* TransitionTree = InTransitionTreeWeak.Get())
	{
		FScopedTransaction Transaction(LOCTEXT("ToggleTransitionTreeEnabled", "Toggle Transition Tree Enabled"));
		TransitionTree->Modify();
		TransitionTree->SetEnabled(!TransitionTree->IsEnabled());
	}
}

bool IsTransitionTreeEnabled(TWeakObjectPtr<UAvaTransitionTree> InTransitionTreeWeak)
{
	return InTransitionTreeWeak.IsValid() && InTransitionTreeWeak->IsEnabled();
}

}

#undef LOCTEXT_NAMESPACE
