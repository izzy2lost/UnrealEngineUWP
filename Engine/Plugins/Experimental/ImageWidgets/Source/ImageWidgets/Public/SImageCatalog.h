// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include <Widgets/SCompoundWidget.h>
#include <Widgets/Views/SListView.h>

namespace UE::ImageWidgets
{
	class SImageCatalogItem;

	/**
	 * Contains all data for a catalog item.
	 */
	struct IMAGEWIDGETS_API FImageCatalogItemData
	{
		FImageCatalogItemData(FGuid Guid, const FSlateBrush& Brush, const FText& Name, const FText& Info, const FText& ToolTip);

		/** Unique identifier for the catalog item */
		FGuid Guid;

		/** Brush used for displaying the item's thumbnail */
		FSlateBrush Thumbnail;

		/** Name of the item */
		FText Name;

		/** Auxiliary information for the item */
		FText Info;

		/** Tooltip that is shown when hovering over any part of the item's widget in the catalog */
		FText ToolTip;
	};

	/**
	 * Generic catalog widget for listing and selecting 2D image-like content.
	 * Each catalog item is represented by its own widget based on its @see FImageCatalogItemData.
	 * Entries in the catalog can be split into regular and pinned items, with pinned items being displayed at the top.
	 */
	class SImageCatalog : public SCompoundWidget
	{
	public:
		/**
		 * Delegate that gets called when an item is selected in the catalog.
		 * The given @see FGuid identifies the item that was selected.
		 */
		DECLARE_DELEGATE_OneParam(FOnItemSelected, const FGuid&)

		/**
		 * Delegate that gets called for creating a context menu for a set of selected items.
		 * Return @see SWidget::NullWidget to not show a context menu.
		 */
		DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FOnGetContextMenu, const TArray<FGuid>&)

		SLATE_BEGIN_ARGS(SImageCatalog)
				: _SelectionMode(ESelectionMode::Multi)
			{
			}

			/** Header text for regular items. */
			SLATE_ATTRIBUTE(FText, ItemsHeading)

			/** Header text for pinned items. */
			SLATE_ATTRIBUTE(FText, PinnedItemsHeading)

			/** Defines the selection behavior within an item list, e.g. only allow single item selection or do not allow any selection. */
			SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)

			/** Delegate that gets called when an item is selected in the catalog. */
			SLATE_EVENT(FOnItemSelected, OnItemSelected)

			/** Delegate that gets called for creating a context menu for a set of selected items. */
			SLATE_EVENT(FOnGetContextMenu, OnGetContextMenu)
		SLATE_END_ARGS()

		/**
		 * Function used by Slate to construct the image catalog widget with the given arguments.
		 * @param InArgs Slate arguments defined above
		 */
		IMAGEWIDGETS_API void Construct(const FArguments& InArgs);

		/**
		 * Adds a regular item to the catalog.
		 * @param Item Data for the item that is being added.
		 */
		IMAGEWIDGETS_API bool AddItem(const TSharedPtr<FImageCatalogItemData>& Item);

		/**
		 * Adds a regular item to the catalog right before another regular item.
		 * If the unique identifier for the other item is invalid, the new item will be added at the end.
		 * @param Item Data for the item that is being added.
		 * @param BeforeItemWithThisGuid Unique identifier of the other item before which the new item should be added. 
		 */
		IMAGEWIDGETS_API bool AddItem(const TSharedPtr<FImageCatalogItemData>& Item, const FGuid& BeforeItemWithThisGuid);

		/**
		 * Adds a pinned item to the catalog. Pinned items appear in a separate list above regular items.
		 * @param Item Data for the item that is being added. 
		 */
		IMAGEWIDGETS_API bool AddPinnedItem(const TSharedPtr<FImageCatalogItemData>& Item);

		/**
		 * Adds a pinned item to the catalog right before another pinned item.
		 * If the unique identifier for the other item is invalid, the new item will be added at the end.
		 * @param Item Data for the item that is being added.
		 * @param BeforeItemWithThisGuid Unique identifier of the other item before which the new item should be added. 
		 */
		IMAGEWIDGETS_API bool AddPinnedItem(const TSharedPtr<FImageCatalogItemData>& Item, const FGuid& BeforeItemWithThisGuid);

		/**
		 * Remove an existing regular or pinned item.
		 * Nothing happens if no item with the given unique identifier exists in the catalog.
		 * @param Guid The unique identifier of the existing item.
		 */
		IMAGEWIDGETS_API bool RemoveItem(const FGuid& Guid);

		/**
		 * Retrieves the existing item for a given unique identifier.
		 * @param Guid The unique identifier of the item.
		 * @return The pointer to the item or an invalid pointer if no item with the given unique identifier exists.
		 */
		IMAGEWIDGETS_API TSharedPtr<const FImageCatalogItemData> GetItem(const FGuid& Guid) const;

		/**
		 * Returns if an existing item is a pinned or regular item as well as the index in the respective item list.
		 * @param Guid The unique identifier of the item.
		 * @return Tuple where the first value indicates if an item is pinned and the second value is the index in the respective item list or no tuple if no
		 * item with the given unique identifier exists.
		 */
		IMAGEWIDGETS_API TOptional<TTuple<bool, int32>> GetItemIndex(const FGuid& Guid) const;

		/**
		 * Retrieves the existing regular item for a given index within the regular item list.
		 * @param Index Index of the regular item; should be at least 0 and less than @see NumItems.
		 * @return The pointer to the item or an invalid pointer if the index is invalid.
		 */
		IMAGEWIDGETS_API TSharedPtr<const FImageCatalogItemData> GetItemAt(int32 Index) const;

		/**
		 * Retrieves the existing pinned item for a given index within the pinned item list.
		 * @param Index Index of the pinned item; should be at least 0 and less than @see NumPinnedItems.
		 * @return The pointer to the item or an invalid pointer if the index is invalid.
		 */
		IMAGEWIDGETS_API TSharedPtr<const FImageCatalogItemData> GetPinnedItemAt(int32 Index) const;
		
		/**
		 * Returns the unique identifier of the item at the given index within the regular item list.
		 * @param Index Index of the regular item; should be at least 0 and less than @see NumItems.
		 * @return Unique identifier of the item or no value if the index is invalid.
		 */
		IMAGEWIDGETS_API TOptional<FGuid> GetItemGuidAt(int32 Index) const;

		/**
		 * Returns the unique identifier of the item at the given index within the pinned item list.
		 * @param Index Index of the pinned item; should be at least 0 and less than @see NumPinnedItems.
		 * @return Unique identifier of the item or no value if the index is invalid.
		 */
		IMAGEWIDGETS_API TOptional<FGuid> GetPinnedItemGuidAt(int32 Index) const;
		
		/**
		 * Returns the number of regular items in the catalog, i.e. items that are not pinned.
		 */
		IMAGEWIDGETS_API int32 NumItems() const;

		/**
		 * Returns the number of pinned items in the catalog.
		 */
		IMAGEWIDGETS_API int32 NumPinnedItems() const;

		/**
		 * Returns the total number of items in the catalog, i.e. both regular items and pinned items.
		 */
		IMAGEWIDGETS_API int32 NumTotalItems() const;

		/**
		 * Check if an item is pinned.
		 * @param Guid The unique identifier of the item.
		 * @return True if the item exists and is pinned. 
		 */
		IMAGEWIDGETS_API bool ItemIsPinned(const FGuid& Guid) const;

		/**
		 * Pin an existing item.
		 * @param Guid The unique identifier of the item.
		 * @return True if the item exists and was not already pinned.
		 */
		IMAGEWIDGETS_API bool PinItem(const FGuid& Guid);

		/**
		 * Unpin an existing item.
		 * @param Guid The unique identifier of the item.
		 * @return True if the item exists and was not already unpinned.
		 */
		IMAGEWIDGETS_API bool UnpinItem(const FGuid& Guid);

		/**
		 * Select an existing regular or pinned item.
		 * Nothing happens if no item with the given unique identifier exists in the catalog.
		 * @param Guid The unique identifier of the existing item.
		 */
		IMAGEWIDGETS_API void SelectItem(const FGuid& Guid);

		/**
		 * Update an existing regular or pinned item's data. The item data should contain the item's unique identifier.
		 * Nothing happens if no item with the given unique identifier exists in the catalog.
		 * @param Item Data for the item that is being updated, including the unique identifier of the existing item.
		 */
		IMAGEWIDGETS_API void UpdateItem(const FImageCatalogItemData& Item);

		/**
		 * Update the info text of an existing regular or pinned item.
		 * Nothing happens if no item with the given unique identifier exists in the catalog.
		 * @param Guid The unique identifier of the existing item.
		 * @param Info Text for the info label in the item's widget.
		 */
		IMAGEWIDGETS_API void UpdateItemInfo(const FGuid& Guid, const FText& Info);

		/**
		 * Update the info text of an existing regular or pinned item.
		 * Nothing happens if no item with the given unique identifier exists in the catalog.
		 * @param Guid The unique identifier of the existing item.
		 * @param Name Text for the name label in the item's widget.
		 */
		IMAGEWIDGETS_API void UpdateItemName(const FGuid& Guid, const FText& Name);

		/**
		 * Update the thumbnail of an existing regular or pinned item.
		 * Nothing happens if no item with the given unique identifier exists in the catalog.
		 * @param Guid The unique identifier of the existing item.
		 * @param Thumbnail Brush used for the thumbnail in the item's widget.
		 */
		IMAGEWIDGETS_API void UpdateItemThumbnail(const FGuid& Guid, const FSlateBrush& Thumbnail);

		/**
		 * Update the tooltip text of an existing regular or pinned item.
		 * Nothing happens if no item with the given unique identifier exists in the catalog.
		 * @param Guid The unique identifier of the existing item.
		 * @param ToolTip Text for the tooltip label in the item's widget.
		 */
		IMAGEWIDGETS_API void UpdateItemToolTip(const FGuid& Guid, const FText& ToolTip);

	private:
		/** Collects the selected items and triggers the callback to create the context menu. */
		TSharedPtr<SWidget> OnContextMenuOpening() const;

		/** Widget for listing all regular items. */
		TSharedPtr<SListView<TSharedPtr<FImageCatalogItemData>>> ItemsListView;

		/** Widget for listing all pinned items. */
		TSharedPtr<SListView<TSharedPtr<FImageCatalogItemData>>> PinnedItemsListView;

		/** Header text for regular items. */
		TAttribute<FText> ItemsHeading;

		/** Header text for pinned items. */
		TAttribute<FText> PinnedItemsHeading;

		/** Delegate that gets called when an item is selected. */
		FOnItemSelected OnItemSelected;

		/** Delegate that gets called to create a context menu for a set of selected items. */
		FOnGetContextMenu OnGetContextMenu;

		/** Internal item storage. */
		TPimplPtr<class FItemModel> Model;
	};
}
