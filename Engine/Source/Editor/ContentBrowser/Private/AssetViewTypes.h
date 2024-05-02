// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "ContentBrowserItem.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Templates/Tuple.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"

class FContentBrowserItemData;
class FString;
class FText;
struct FAssetViewCustomColumn;

/** An item (folder or file) displayed in the asset view */
class FAssetViewItem
{
public:
	FAssetViewItem() = default;

	explicit FAssetViewItem(FContentBrowserItem&& InItem);
	explicit FAssetViewItem(const FContentBrowserItem& InItem);

	explicit FAssetViewItem(FContentBrowserItemData&& InItemData);
	explicit FAssetViewItem(const FContentBrowserItemData& InItemData);

	// When recycling an object, clear the item data and replace it with the given data.
	void ResetItemData(FContentBrowserItemData InItemData);

	void AppendItemData(const FContentBrowserItem& InItem);

	void AppendItemData(const FContentBrowserItemData& InItemData);

	void RemoveItemData(const FContentBrowserItem& InItem);

	void RemoveItemData(const FContentBrowserItemData& InItemData);

	void RemoveItemData(const FContentBrowserMinimalItemData& InItemKey);

	/** Clear cached custom column data */
	void ClearCachedCustomColumns();

	/**
	 * Updates cached custom column data (only does something for files) 
	 * @param bUpdateExisting If true, only updates existing columns, if false only adds missing columns 
	 */
	void CacheCustomColumns(TArrayView<const FAssetViewCustomColumn> CustomColumns, const bool bUpdateSortData, const bool bUpdateDisplayText, const bool bUpdateExisting);
	
	/** Get the display value of a custom column on this item */
	bool GetCustomColumnDisplayValue(const FName ColumnName, FText& OutText) const;

	/** Get the value (and optionally also the type) of a custom column on this item */
	bool GetCustomColumnValue(const FName ColumnName, FString& OutString, UObject::FAssetRegistryTag::ETagType* OutType = nullptr) const;

	/** Get the value (and optionally also the type) of a named tag on this item */
	bool GetTagValue(const FName Tag, FString& OutString, UObject::FAssetRegistryTag::ETagType* OutType = nullptr) const;

	/** Get the underlying Content Browser item */
	const FContentBrowserItem& GetItem() const;

	bool IsFolder() const;

	bool IsFile() const;

	bool IsTemporary() const;

	// Called when the view explicitly wants to notify widgets of changes
	// Not called during bulk rebuilds when the view will be re-populated even if items are being recycled
	void BroadcastItemDataChanged();

	/** Get the event fired when the data for this item changes */
	FSimpleMulticastDelegate& OnItemDataChanged();

	/** Get the event fired whenever a rename is requested */
	FSimpleDelegate& OnRenameRequested();

	/** Get the event fired whenever a rename is canceled */
	FSimpleDelegate& OnRenameCanceled();

	/** Helper function to turn an item into a string for debugging */
	static FString ItemToString_Debug(TSharedPtr<FAssetViewItem> AssetItem);

private:
	/** Underlying Content Browser item data */
	FContentBrowserItem Item;

	/** An event to fire when the data for this item changes */
	FSimpleMulticastDelegate ItemDataChangedEvent;

	/** Broadcasts whenever a rename is requested */
	FSimpleDelegate RenameRequestedEvent;

	/** Broadcasts whenever a rename is canceled */
	FSimpleDelegate RenameCanceledEvent;

	/** Map of values/types for custom columns */
	TMap<FName, TTuple<FString, UObject::FAssetRegistryTag::ETagType>> CachedCustomColumnData;
	
	/** Map of display text for custom columns */
	TMap<FName, FText> CachedCustomColumnDisplayText;
};
