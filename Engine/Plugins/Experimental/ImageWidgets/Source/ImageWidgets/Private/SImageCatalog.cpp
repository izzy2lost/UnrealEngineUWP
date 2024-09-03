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
FImageCatalogItemData::FImageCatalogItemData(const FGuid Guid, const FSlateBrush& Brush, const FText& Name, const FText& Info, const FText& ToolTip)
	: Guid(Guid), Thumbnail(Brush), Name(Name), Info(Info), ToolTip(ToolTip)
{
}

using FItemType = TSharedPtr<FImageCatalogItemData>;
	
class FItemModel
{
public:

	bool Add(const FItemType& Item, bool bIsPinned, const FGuid* BeforeThisGuid);
	TTuple<bool, bool> Remove(const FGuid& Guid);

	TTuple<const FItemType*, bool> GetItem(const FGuid& Guid) const;
	const FItemType* GetItemAt(int32 Index, bool bIsPinned) const;
	TOptional<TTuple<bool, int32>> GetItemIndex(const FGuid& Guid) const;
	TOptional<FGuid> GetGuidAt(int32 Index, bool bIsPinned) const;
	
	bool IsPinned(const FGuid& Guid);
	bool Pin(const FGuid& Guid);
	bool Unpin(const FGuid& Guid);
	
	const TArray<FItemType>& GetPinnedItems() const { return PinnedItems; }
	const TArray<FItemType>& GetUnpinnedItems() const { return UnpinnedItems; }
	bool HasPinnedItems() const { return PinnedItems.Num() > 0; }
	bool HasUnpinnedItems() const { return UnpinnedItems.Num() > 0; }
	int32 NumPinnedItems() const { return PinnedItems.Num(); }
	int32 NumUnpinnedItems() const { return UnpinnedItems.Num(); }

	void SortSelection(TArray<FGuid>& Selection);

private:

	struct FLookupData
	{
		bool bIsPinned;	// Is this item pinned?
		int32 Index;	// Index in the respective array.
	};

	const FLookupData* FindLookupData(const FGuid& Guid) const;
	FLookupData* FindLookupData(const FGuid& Guid)
	{
		return const_cast<FLookupData*>(const_cast<const FItemModel*>(this)->FindLookupData(Guid));
	}

	const FItemType* GetItem(const FLookupData& LookupData) const;

	void SwapBetweenPinnedAndUnpinned(FLookupData& LookupData);
	void UpdateMappingIndices(const TArray<FItemType>& Container, int32 FirstIndex);

	TArray<FItemType> PinnedItems;
	TArray<FItemType> UnpinnedItems;
	TMap<FGuid, FLookupData> GuidToLookupDataMapping;
};

bool FItemModel::Add(const FItemType& Item, const bool bIsPinned, const FGuid* BeforeThisGuid)
{
	if (!GuidToLookupDataMapping.Contains(Item->Guid))
	{
		TArray<FItemType>& Container = bIsPinned ? PinnedItems : UnpinnedItems;

		const int32 Index = [this, &Item, bIsPinned, BeforeThisGuid, &Container]
		{
			if (BeforeThisGuid)
			{
				if (FLookupData *const LookupData = FindLookupData(*BeforeThisGuid))
				{
					if (LookupData->bIsPinned == bIsPinned)
					{
						// Set index for added item.
						const int32 NewIndex = LookupData->Index;

						// Increase index for the item we push back.
						++LookupData->Index;

						// Add item at the new index.
						Container.EmplaceAt(NewIndex, Item);

						// Update all lookup data for items that come after the item we used to determine the insert location.
						// This way we save the effort for finding the same lookup data again.
						UpdateMappingIndices(Container, LookupData->Index + 1);

						// Tell the outside where the new item was added.
						return NewIndex;
					}
				}
			}
			
			return Container.Add(Item);
		}();

		GuidToLookupDataMapping.Add(Item->Guid, {bIsPinned, Index});
		return true;
	}

	return false;
}

TTuple<bool, bool> FItemModel::Remove(const FGuid& Guid)
{
	if (const FLookupData *const LookupData = FindLookupData(Guid))
	{
		const bool bIsPinned = LookupData->bIsPinned;
		TArray<FItemType>& Container = bIsPinned ? PinnedItems : UnpinnedItems;
		Container.RemoveAt(LookupData->Index, EAllowShrinking::No);
		GuidToLookupDataMapping.Remove(Guid);
		UpdateMappingIndices(Container, LookupData->Index);
		return {true, bIsPinned};
	}

	return {false, false};
}

TTuple<const FItemType*, bool> FItemModel::GetItem(const FGuid& Guid) const
{
	if (const FLookupData *const LookupData = FindLookupData(Guid))
	{
		return {GetItem(*LookupData), LookupData->bIsPinned};
	}

	return {nullptr, false};
}

const FItemType* FItemModel::GetItemAt(const int32 Index, const bool bIsPinned) const
{
	const TArray<FItemType>& Container = bIsPinned ? PinnedItems : UnpinnedItems;
	if (0 <= Index && Index < Container.Num())
	{
		return &Container[Index];
	}

	return nullptr;
}

TOptional<TTuple<bool, int32>> FItemModel::GetItemIndex(const FGuid& Guid) const
{
	if (const FLookupData *const LookupData = FindLookupData(Guid))
	{
		return {{LookupData->bIsPinned, LookupData->Index}};
	}

	return {};
}

TOptional<FGuid> FItemModel::GetGuidAt(const int32 Index, const bool bIsPinned) const
{
	const TArray<FItemType>& Container = bIsPinned ? PinnedItems : UnpinnedItems;
	if (0 <= Index && Index < Container.Num())
	{
		return Container[Index]->Guid;
	}

	return {};
}

bool FItemModel::IsPinned(const FGuid& Guid)
{
	const FLookupData *const LookupData = FindLookupData(Guid);
	return LookupData && LookupData->bIsPinned;
}

bool FItemModel::Pin(const FGuid& Guid)
{
	FLookupData *const LookupData = FindLookupData(Guid);
	if (LookupData && !LookupData->bIsPinned)
	{
		SwapBetweenPinnedAndUnpinned(*LookupData);
		return true;
	}

	return false;
}

bool FItemModel::Unpin(const FGuid& Guid)
{
	FLookupData *const LookupData = FindLookupData(Guid);
	if (LookupData && LookupData->bIsPinned)
	{
		SwapBetweenPinnedAndUnpinned(*LookupData);
		return true;
	}

	return false;
}

void FItemModel::SortSelection(TArray<FGuid>& Selection)
{
	Algo::Sort(Selection, [this, NumPinned = PinnedItems.Num()](const FGuid& A, const FGuid& B)
	{
		const FLookupData *const LookupDataA = FindLookupData(A);
		const FLookupData *const LookupDataB = FindLookupData(B);

		if (LookupDataA && !LookupDataB) return true;
		if (!LookupDataA && LookupDataB) return false;
		if (!LookupDataA && !LookupDataB) return true;

		const int32 PositionA = (LookupDataA->bIsPinned ? 0 : NumPinned) + LookupDataA->Index;
		const int32 PositionB = (LookupDataB->bIsPinned ? 0 : NumPinned) + LookupDataB->Index;
		
		return PositionA < PositionB;
	});
}

const FItemModel::FLookupData* FItemModel::FindLookupData(const FGuid& Guid) const
{
	if (const FLookupData* LookupData = GuidToLookupDataMapping.Find(Guid))
	{
		return LookupData;
	}

	UE_LOG(LogImageWidgets, Warning, TEXT("Cannot find catalog item for guid '%s'."), *Guid.ToString());
	return nullptr;
}

const FItemType* FItemModel::GetItem(const FLookupData& LookupData) const
{
	const TArray<FItemType> *const Container = LookupData.bIsPinned ? &PinnedItems : &UnpinnedItems;
	check(0 <= LookupData.Index && LookupData.Index < Container->Num());
	return &(*Container)[LookupData.Index];
}

void FItemModel::SwapBetweenPinnedAndUnpinned(FLookupData& LookupData)
{
	TArray<FItemType>& From = LookupData.bIsPinned ? PinnedItems : UnpinnedItems;
	TArray<FItemType>& To = LookupData.bIsPinned ? UnpinnedItems : PinnedItems;
	
	const int32 NewIndex = To.Emplace(static_cast<FItemType&&>(From[LookupData.Index]));
	From.RemoveAt(LookupData.Index, EAllowShrinking::No);

	UpdateMappingIndices(From, LookupData.Index);

	LookupData.Index = NewIndex;
	LookupData.bIsPinned = !LookupData.bIsPinned;
}

void FItemModel::UpdateMappingIndices(const TArray<FItemType>& Container, const int32 FirstIndex)
{
	for (int32 Index = FirstIndex, Num = Container.Num(); Index < Num; ++Index)
	{
		FLookupData *const LookupData = GuidToLookupDataMapping.Find(Container[Index]->Guid);
		check(LookupData);
		LookupData->Index = Index;
	}
}

void SImageCatalog::Construct(const FArguments& InArgs)
{
	Model = MakePimpl<FItemModel>();
	
	ItemsHeading = InArgs._ItemsHeading;
	PinnedItemsHeading = InArgs._PinnedItemsHeading;
	OnItemSelected = InArgs._OnItemSelected;
	OnGetContextMenu = InArgs._OnGetContextMenu;

	const auto GenerateItemRow = [](const FItemType& ItemData, const TSharedRef<STableViewBase>& OwnerTable)
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

		return SNew(STableRow<FItemType>, OwnerTable)
			.Style(&TableRowStyle)
			.ShowSelection(true)
			[
				ItemWidget.ToSharedRef()
			];
	};

	const auto GetPinnedItemsVisibility = [&Model = Model]
	{
		return Model->HasPinnedItems() ? EVisibility::Visible : EVisibility::Collapsed;
	};

	const auto GetItemsVisibility = [&Model = Model]
	{
		return Model->HasUnpinnedItems() ? EVisibility::Visible : EVisibility::Collapsed;
	};

	const auto PinnedItemsSelectionChanged = [this](const FItemType& Item, ESelectInfo::Type SelectInfo)
	{
		// Note that Item might be a nullptr since this callback is also executed when clearing the selection of the list.
		if (Item.IsValid())
		{
			ItemsListView->ClearSelection();
			OnItemSelected.ExecuteIfBound(Item->Guid);
		}
	};

	const auto ItemsSelectionChanged = [this](const FItemType& Item, ESelectInfo::Type SelectInfo)
	{
		// Note that Item might be a nullptr since this callback is also executed when clearing the selection of the list.
		if (Item.IsValid())
		{
			PinnedItemsListView->ClearSelection();
			OnItemSelected.ExecuteIfBound(Item->Guid);
		}
	};

	SAssignNew(PinnedItemsListView, SListView<FItemType>)
					.ListItemsSource(&Model->GetPinnedItems())
					.OnContextMenuOpening(this, &SImageCatalog::OnContextMenuOpening)
					.OnGenerateRow_Lambda(GenerateItemRow)
					.SelectionMode(InArgs._SelectionMode)
					.OnSelectionChanged_Lambda(PinnedItemsSelectionChanged)
					.ClearSelectionOnClick(false)
					.Visibility_Lambda(GetPinnedItemsVisibility);

	SAssignNew(ItemsListView, SListView<FItemType>)
					.ListItemsSource(&Model->GetUnpinnedItems())
					.OnContextMenuOpening(this, &SImageCatalog::OnContextMenuOpening)
					.OnGenerateRow_Lambda(GenerateItemRow)
					.SelectionMode(InArgs._SelectionMode)
					.OnSelectionChanged_Lambda(ItemsSelectionChanged)
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
					.Text_Lambda([&PinnedItemsHeading = PinnedItemsHeading] { return PinnedItemsHeading.Get({}); })
					.Visibility_Lambda([&PinnedItemsHeading = PinnedItemsHeading, GetPinnedItemsVisibility]
					{
						return PinnedItemsHeading.Get({}).IsEmpty() ? EVisibility::Collapsed : GetPinnedItemsVisibility();
					})
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
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
					.Visibility_Lambda(GetPinnedItemsVisibility)
			]
		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 4.0f, 2.0f, 4.0f)
			[
				SNew(STextBlock)
					.Text_Lambda([&ItemsHeading = ItemsHeading] { return ItemsHeading.Get({}); })
					.Visibility_Lambda([&ItemsHeading = ItemsHeading, GetItemsVisibility]
					{
						return ItemsHeading.Get({}).IsEmpty() ? EVisibility::Collapsed : GetItemsVisibility();
					})
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
			]
		+ SVerticalBox::Slot()
			[
				ItemsListView.ToSharedRef()
			]
	];
}

bool SImageCatalog::AddItem(const FItemType& Item)
{
	if (Model->Add(Item, false, nullptr))
	{
		ItemsListView->RequestListRefresh();
		return true;
	}

	return false;
}

bool SImageCatalog::AddItem(const TSharedPtr<FImageCatalogItemData>& Item, const FGuid& BeforeItemWithThisGuid)
{
	if (Model->Add(Item, false, &BeforeItemWithThisGuid))
	{
		ItemsListView->RequestListRefresh();
		return true;
	}

	return false;
}

bool SImageCatalog::AddPinnedItem(const FItemType& Item)
{
	if (Model->Add(Item, true, nullptr))
	{
		PinnedItemsListView->RequestListRefresh();
		return true;
	}

	return false;
}

bool SImageCatalog::AddPinnedItem(const TSharedPtr<FImageCatalogItemData>& Item, const FGuid& BeforeItemWithThisGuid)
{
	if (Model->Add(Item, true, &BeforeItemWithThisGuid))
	{
		PinnedItemsListView->RequestListRefresh();
		return true;
	}

	return false;
}

bool SImageCatalog::RemoveItem(const FGuid& Guid)
{
	const auto [bSuccess, bIsPinned] = Model->Remove(Guid);
	if (bSuccess)
	{
		const TSharedPtr<SListView<FItemType>>& ListView = bIsPinned ? PinnedItemsListView : ItemsListView;
		ListView->RequestListRefresh();
		return true;
	}

	return false;
}

TSharedPtr<const FImageCatalogItemData> SImageCatalog::GetItem(const FGuid& Guid) const
{
	if (const FItemType* ItemPtr = Model->GetItem(Guid).Get<0>())
	{
		return *ItemPtr;
	}
	return {};
}

TOptional<TTuple<bool, int32>> SImageCatalog::GetItemIndex(const FGuid& Guid) const
{
	return Model->GetItemIndex(Guid);
}

TSharedPtr<const FImageCatalogItemData> SImageCatalog::GetItemAt(const int32 Index) const
{
	if (const FItemType* ItemPtr = Model->GetItemAt(Index, false))
	{
		return *ItemPtr;
	}
	return {};
}

TSharedPtr<const FImageCatalogItemData> SImageCatalog::GetPinnedItemAt(const int32 Index) const
{
	if (const FItemType* ItemPtr = Model->GetItemAt(Index, true))
	{
		return *ItemPtr;
	}
	return {};
}

TOptional<FGuid> SImageCatalog::GetItemGuidAt(const int32 Index) const
{
	return Model->GetGuidAt(Index, false);
}

TOptional<FGuid> SImageCatalog::GetPinnedItemGuidAt(const int32 Index) const 
{
	return Model->GetGuidAt(Index, true);
}

int32 SImageCatalog::NumItems() const
{
	return Model->NumUnpinnedItems();
}

int32 SImageCatalog::NumPinnedItems() const
{
	return Model->NumPinnedItems();
}

int32 SImageCatalog::NumTotalItems() const
{
	return NumItems() + NumPinnedItems();
}

bool SImageCatalog::ItemIsPinned(const FGuid& Guid) const
{
	return Model->IsPinned(Guid);
}
	
bool SImageCatalog::PinItem(const FGuid& Guid)
{
	const bool bSuccess = Model->Pin(Guid);
	if (bSuccess)
	{
		PinnedItemsListView->RequestListRefresh();
		ItemsListView->RequestListRefresh();
	}
	return bSuccess;
}

bool SImageCatalog::UnpinItem(const FGuid& Guid)
{
	const bool bSuccess = Model->Unpin(Guid);
	if (bSuccess)
	{
		PinnedItemsListView->RequestListRefresh();
		ItemsListView->RequestListRefresh();
	}
	return bSuccess;
}

void SImageCatalog::SelectItem(const FGuid& Guid)
{
	const auto [ItemPtr, bIsPinned] = Model->GetItem(Guid);
	if (ItemPtr)
	{
		ItemsListView->ClearSelection();
		PinnedItemsListView->ClearSelection();

		SListView<FItemType> *const ListView = bIsPinned ? PinnedItemsListView.Get() : ItemsListView.Get();
		ListView->SetItemSelection(*ItemPtr, true);
	}
}

TSharedPtr<SWidget> SImageCatalog::OnContextMenuOpening() const
{
	if (!OnGetContextMenu.IsBound())
	{
		return SNullWidget::NullWidget;
	}

	TArray<FGuid> SelectedGuids = [&PinnedItemsListView = PinnedItemsListView, &ItemsListView = ItemsListView]
	{
		TArray<FGuid> Guids;
		auto GetItemGuid = [](const FItemType& Item) { return Item->Guid; };
		Algo::Transform(PinnedItemsListView->GetSelectedItems(), Guids, GetItemGuid);
		Algo::Transform(ItemsListView->GetSelectedItems(), Guids, GetItemGuid);
		return Guids;
	}();

	if (SelectedGuids.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}

	Model->SortSelection(SelectedGuids);

	return OnGetContextMenu.Execute(SelectedGuids);
}

void SImageCatalog::UpdateItem(const FImageCatalogItemData& Item)
{
	if (const TSharedPtr<FImageCatalogItemData>* const ItemPtr = Model->GetItem(Item.Guid).Get<0>())
	{
		**ItemPtr = Item;
	}
}

void SImageCatalog::UpdateItemInfo(const FGuid& Guid, const FText& Info)
{
	if (const TSharedPtr<FImageCatalogItemData>* const ItemPtr = Model->GetItem(Guid).Get<0>())
	{
		(*ItemPtr)->Info = Info;
	}
}

void SImageCatalog::UpdateItemName(const FGuid& Guid, const FText& Name)
{
	if (const TSharedPtr<FImageCatalogItemData>* const ItemPtr = Model->GetItem(Guid).Get<0>())
	{
		(*ItemPtr)->Name = Name;
	}
}

void SImageCatalog::UpdateItemThumbnail(const FGuid& Guid, const FSlateBrush& Thumbnail)
{
	if (const TSharedPtr<FImageCatalogItemData>* const ItemPtr = Model->GetItem(Guid).Get<0>())
	{
		(*ItemPtr)->Thumbnail = Thumbnail;
	}
}

void SImageCatalog::UpdateItemToolTip(const FGuid& Guid, const FText& ToolTip)
{
	if (const TSharedPtr<FImageCatalogItemData>* const ItemPtr = Model->GetItem(Guid).Get<0>())
	{
		(*ItemPtr)->ToolTip = ToolTip;
	}
}
}

#undef LOCTEXT_NAMESPACE
