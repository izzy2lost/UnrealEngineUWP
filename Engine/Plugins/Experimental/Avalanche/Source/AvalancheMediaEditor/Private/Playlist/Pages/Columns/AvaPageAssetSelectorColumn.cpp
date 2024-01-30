// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaPageAssetSelectorColumn.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AvaBlueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaPageAssetSelectorColumn"

FText FAvaPageAssetSelectorColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("AssetSelectorColumn_Name", "Motion Design Asset");
}

FText FAvaPageAssetSelectorColumn::GetColumnToolTipText() const
{
	return LOCTEXT("AssetSelectorColumn_ToolTip", "Selects a given Motion Design Asset for the Page");
}

SHeaderRow::FColumn::FArguments FAvaPageAssetSelectorColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FillWidth(0.25f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
	;
}

namespace UE::AvaPageAssetSelectorColumn::Private
{
	TSharedRef<SWidget> GetAssetPicker(const FAvaPageViewRef& InPageView)
	{
		const UAvalanchePlaylist* Playlist = InPageView->GetPlaylist();

		constexpr bool bAllowClear = true;
		TArray<const UClass*> AllowedClasses;
		AllowedClasses.Add(UAvalancheBlueprint::StaticClass());
		AllowedClasses.Add(UWorld::StaticClass());

		const FAssetData AssetData = IAssetRegistry::Get()->GetAssetByObjectPath(InPageView->GetObjectPath(Playlist));
	
		return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
			AssetData,
			bAllowClear,
			AllowedClasses,
			PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(AllowedClasses),
			FOnShouldFilterAsset(),
			FOnAssetSelected::CreateSP(InPageView, &IAvaPageView::OnObjectChanged),
			FSimpleDelegate::CreateLambda([]{FSlateApplication::Get().DismissAllMenus();}));
	}
}

TSharedRef<SWidget> FAvaPageAssetSelectorColumn::ConstructRowWidget(const FAvaPageViewRef& InPageView,
                                                                    const TSharedPtr<SAvaPageViewRow>& InRow)
{
	const UAvalanchePlaylist* Playlist = InPageView->GetPlaylist();

	// Combo templates don't have an asset selector.
	// Put a place holder instead.
	if (!InPageView->HasObjectPath(Playlist))
	{
		return SNew(STextBlock)
		.Text(LOCTEXT("AssetSelectorColumn_ComboPage", "N/A"))
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	}
	
	return SNew(SComboButton)
			.OnGetMenuContent_Lambda([InPageView]{ return UE::AvaPageAssetSelectorColumn::Private::GetAssetPicker(InPageView);})
			.ContentPadding(FMargin(2.0f, 2.0f))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(InPageView, &IAvaPageView::GetObjectName, Playlist)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
}

#undef LOCTEXT_NAMESPACE
