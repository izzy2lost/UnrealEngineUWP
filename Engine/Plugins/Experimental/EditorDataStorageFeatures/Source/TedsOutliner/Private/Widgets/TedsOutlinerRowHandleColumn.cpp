// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsOutlinerRowHandleColumn.h"

#include "SortHelper.h"
#include "TypedElementOutlinerItem.h"

#define LOCTEXT_NAMESPACE "TedsOutlinerRowHandleColumn"

FName FTedsOutlinerRowHandleColumn::GetID()
{
	static const FName ID("Row Handle");
	return ID;
}

FName FTedsOutlinerRowHandleColumn::GetColumnID()
{
	return GetID();
}

SHeaderRow::FColumn::FArguments FTedsOutlinerRowHandleColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetID())
	.FillWidth(2)
	.HeaderComboVisibility(EHeaderComboVisibility::OnHover);
}

const TSharedRef<SWidget> FTedsOutlinerRowHandleColumn::ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row)
{
	auto SceneOutliner = WeakSceneOutliner.Pin();
	check(SceneOutliner.IsValid());

	if (const FTypedElementOutlinerTreeItem* OutlinerTreeItem = TreeItem->CastTo<FTypedElementOutlinerTreeItem>())
	{
		const TypedElementDataStorage::RowHandle RowHandle = OutlinerTreeItem->GetRowHandle();

		FNumberFormattingOptions NumberFormattingOptions;
		NumberFormattingOptions.SetUseGrouping(false);
		const FText Text = FText::AsNumber(RowHandle, &NumberFormattingOptions);
		
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			.Content()
			[
				SNew(STextBlock)
					.Text(Text)
					.HighlightText(SceneOutliner->GetFilterHighlightText())
					.ColorAndOpacity(FSlateColor::UseForeground())
			];
	}
	return SNullWidget::NullWidget;
}

void FTedsOutlinerRowHandleColumn::PopulateSearchStrings(const ISceneOutlinerTreeItem& Item, TArray<FString>& OutSearchStrings) const
{
	if (const FTypedElementOutlinerTreeItem* OutlinerTreeItem = Item.CastTo<FTypedElementOutlinerTreeItem>())
	{
		OutSearchStrings.Add(LexToString<FString>(OutlinerTreeItem->GetRowHandle()));
	}

}

void FTedsOutlinerRowHandleColumn::SortItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems, const EColumnSortMode::Type SortMode) const
{
	FSceneOutlinerSortHelper<TypedElementDataStorage::RowHandle>()
		/** Sort by type first */
		.Primary([this](const ISceneOutlinerTreeItem& Item)
		{
			if (const FTypedElementOutlinerTreeItem* OutlinerTreeItem = Item.CastTo<FTypedElementOutlinerTreeItem>())
			{
				return OutlinerTreeItem->GetRowHandle();
			}

			return TypedElementDataStorage::InvalidRowHandle;
		}, SortMode)
		.Sort(OutItems);
}

#undef LOCTEXT_NAMESPACE
