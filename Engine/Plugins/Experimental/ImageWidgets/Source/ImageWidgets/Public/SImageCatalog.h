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
	 * Each catalog item is represented by its own widget based on its @ref FImageCatalogItemData.
	 * Entries in the catalog can be split into regular and pinned items, with pinned items being displayed at the top.
	 */
	class SImageCatalog : public SCompoundWidget
	{
	public:
		/**
		 * Delegate that gets called when an item is selected in the catalog.
		 * The given @ref FGuid identifies the item that was selected.
		 */
		DECLARE_DELEGATE_OneParam(FOnItemSelected, const FGuid&)

		SLATE_BEGIN_ARGS(SImageCatalog)
			{
			}

			/** Header text for regular items */
			SLATE_ARGUMENT(FText, ItemsHeading)

			/** Header text for pinned items */
			SLATE_ARGUMENT(FText, PinnedItemsHeading)

			/** Delegate that gets called when an item is selected in the catalog. */
			SLATE_EVENT(FOnItemSelected, OnItemSelected)
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
		IMAGEWIDGETS_API void AddItem(const TSharedPtr<FImageCatalogItemData>& Item);

		/**
		 * Adds a pinned item to the catalog. Pinned items appear in a seperate list above regular items.
		 * @param Item Data for the item that is being added. 
		 */
		IMAGEWIDGETS_API void AddPinnedItem(const TSharedPtr<FImageCatalogItemData>& Item);

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
		/** Find an existing regular or pinned item's data. The second value is true if the item is pinned. */
		const TPair<const TSharedPtr<FImageCatalogItemData>, bool>* FindItemData(const FGuid& Guid);

		/** Widget for listing all regular items. */
		TSharedPtr<SListView<TSharedPtr<FImageCatalogItemData>>> ItemsListView;

		/** Widget for listing all pinned items. */
		TSharedPtr<SListView<TSharedPtr<FImageCatalogItemData>>> PinnedItemsListView;

		/** Regular item data. */
		TArray<TSharedPtr<FImageCatalogItemData>> Items;

		/** Regular item data. */
		TArray<TSharedPtr<FImageCatalogItemData>> PinnedItems;

		/** Mapping from the items' unique identifier to their respective data and if it is a pinned item. */
		TMap<FGuid, TPair<const TSharedPtr<FImageCatalogItemData>, bool>> GuidToItemMapping;

		/** Delegate that gets called when an item is selected. */
		FOnItemSelected OnItemSelected;
	};
}
