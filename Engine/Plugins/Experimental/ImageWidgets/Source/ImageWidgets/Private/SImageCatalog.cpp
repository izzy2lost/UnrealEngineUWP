// Copyright Epic Games, Inc. All Rights Reserved.

#include "SImageCatalog.h"

#include "ImageWidgetsLogCategory.h"
#include "SImageCatalogItem.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "SImageViewerCatalog"

namespace UE::ImageWidgets
{
FImageCatalogItemData::FImageCatalogItemData(FGuid Guid, const FSlateBrush& Brush, const FText& Name, const FText& Info, const FText& ToolTip)
	: Guid(Guid), Thumbnail(Brush), Name(Name), Info(Info), ToolTip(ToolTip)
{
}

void SImageCatalog::Construct(const FArguments& InArgs)
{
	OnItemSelected = InArgs._OnItemSelected;

	Items.Reserve(1000);

	const auto GenerateItemRow = [](const TSharedPtr<FImageCatalogItemData>& ItemData, const TSharedRef<STableViewBase>& OwnerTable)
	{
		static const FTableRowStyle TableRowStyle = []
		{
			FTableRowStyle Style = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
			Style.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Background));
			Style.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::SelectHover));
			Style.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Recessed));
			Style.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::SelectHover));
			return Style;
		}();

		TSharedPtr<SImageCatalogItem> ItemWidget;
		SAssignNew(ItemWidget, SImageCatalogItem, ItemData);

		return SNew(STableRow<TSharedPtr<FImageCatalogItemData>>, OwnerTable)
			.Style(&TableRowStyle)
			.ShowSelection(true)
			[
				ItemWidget.ToSharedRef()
			];
	};

	const auto HasPinnedItems = [this]
	{
		return PinnedItems.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
	};

	const auto PinnedItemSelectionChanged = [this](const TSharedPtr<FImageCatalogItemData>& Item, ESelectInfo::Type SelectInfo)
	{
		// Note that Item might be a nullptr since this callback is also executed when clearing the selection of the list.
		if (Item.IsValid() && OnItemSelected.IsBound())
		{
			ItemsListView->ClearSelection();
			OnItemSelected.Execute(Item->Guid);
		}
	};

	const auto ItemSelectionChanged = [this](const TSharedPtr<FImageCatalogItemData>& Item, ESelectInfo::Type SelectInfo)
	{
		// Note that Item might be a nullptr since this callback is also executed when clearing the selection of the list.
		if (Item.IsValid() && OnItemSelected.IsBound())
		{
			PinnedItemsListView->ClearSelection();
			OnItemSelected.Execute(Item->Guid);
		}
	};

	SAssignNew(PinnedItemsListView, SListView<TSharedPtr<FImageCatalogItemData>>)
					.ListItemsSource(&PinnedItems)
					.OnGenerateRow_Lambda(GenerateItemRow)
					.OnSelectionChanged_Lambda(PinnedItemSelectionChanged)
					.ClearSelectionOnClick(false)
					.Visibility_Lambda(HasPinnedItems);

	SAssignNew(ItemsListView, SListView<TSharedPtr<FImageCatalogItemData>>)
					.ListItemsSource(&Items)
					.OnGenerateRow_Lambda(GenerateItemRow)
					.OnSelectionChanged_Lambda(ItemSelectionChanged)
					.ClearSelectionOnClick(false)
					.ScrollbarVisibility(EVisibility::Visible);
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 4.0f, 2.0f, 4.0f)
			[
				SNew(STextBlock)
					.Text(InArgs._PinnedItemsHeading)
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Visibility(InArgs._PinnedItemsHeading.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			]
		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				PinnedItemsListView.ToSharedRef()
			]
		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
					.Thickness(6.0f)
					.Visibility_Lambda(HasPinnedItems)
			]
		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 4.0f, 2.0f, 4.0f)
			[
				SNew(STextBlock)
					.Text(InArgs._ItemsHeading)
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Visibility(InArgs._ItemsHeading.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			]
		+ SVerticalBox::Slot()
			[
				ItemsListView.ToSharedRef()
			]
	];
}

void SImageCatalog::AddItem(const TSharedPtr<FImageCatalogItemData>& Item)
{
	Items.Add(Item);
	GuidToItemMapping.Add(Item->Guid, TPair<const TSharedPtr<FImageCatalogItemData>, bool>(Item, false));
	ItemsListView->ReGenerateItems(GetCachedGeometry());
}

void SImageCatalog::AddPinnedItem(const TSharedPtr<FImageCatalogItemData>& Item)
{
	PinnedItems.Add(Item);
	GuidToItemMapping.Add(Item->Guid, TPair<const TSharedPtr<FImageCatalogItemData>, bool>(Item, true));
	PinnedItemsListView->ReGenerateItems(GetCachedGeometry());
}

int32 SImageCatalog::NumItems() const
{
	return Items.Num();
}

int32 SImageCatalog::NumPinnedItems() const
{
	return PinnedItems.Num();
}

int32 SImageCatalog::NumTotalItems() const
{
	return NumItems() + NumPinnedItems();
}

void SImageCatalog::SelectItem(const FGuid& Guid)
{
	if (const TPair<const TSharedPtr<FImageCatalogItemData>, bool> *const ExistingItem = FindItemData(Guid))
	{
		ItemsListView->ClearSelection();
		PinnedItemsListView->ClearSelection();

		SListView<TSharedPtr<FImageCatalogItemData>>* ListView = ExistingItem->Get<1>() ? PinnedItemsListView.Get() : ItemsListView.Get();

		ListView->SetItemSelection(ExistingItem->Get<0>(), true);
	}
}

const TPair<const TSharedPtr<FImageCatalogItemData>, bool>* SImageCatalog::FindItemData(const FGuid& Guid)
{
	if (const TPair<const TSharedPtr<FImageCatalogItemData>, bool>* ItemPtr = GuidToItemMapping.Find(Guid))
	{
		return ItemPtr;
	}

	UE_LOG(LogImageWidgets, Warning, TEXT("Cannot find catalog item for guid '%s'."), *Guid.ToString());
	return nullptr;
}

void SImageCatalog::UpdateItem(const FImageCatalogItemData& Item)
{
	if (const TPair<const TSharedPtr<FImageCatalogItemData>, bool> *const ExistingItem = FindItemData(Item.Guid))
	{
		*ExistingItem->Get<0>() = Item;
	}
}

void SImageCatalog::UpdateItemInfo(const FGuid& Guid, const FText& Info)
{
	if (const TPair<const TSharedPtr<FImageCatalogItemData>, bool> *const ExistingItem = FindItemData(Guid))
	{
		ExistingItem->Get<0>()->Info = Info;
	}
}

void SImageCatalog::UpdateItemName(const FGuid& Guid, const FText& Name)
{
	if (const TPair<const TSharedPtr<FImageCatalogItemData>, bool> *const ExistingItem = FindItemData(Guid))
	{
		ExistingItem->Get<0>()->Name = Name;
	}
}

void SImageCatalog::UpdateItemThumbnail(const FGuid& Guid, const FSlateBrush& Thumbnail)
{
	if (const TPair<const TSharedPtr<FImageCatalogItemData>, bool> *const ExistingItem = FindItemData(Guid))
	{
		ExistingItem->Get<0>()->Thumbnail = Thumbnail;
	}
}

void SImageCatalog::UpdateItemToolTip(const FGuid& Guid, const FText& ToolTip)
{
	if (const TPair<const TSharedPtr<FImageCatalogItemData>, bool> *const ExistingItem = FindItemData(Guid))
	{
		ExistingItem->Get<0>()->ToolTip = ToolTip;
	}
}
}

#undef LOCTEXT_NAMESPACE
