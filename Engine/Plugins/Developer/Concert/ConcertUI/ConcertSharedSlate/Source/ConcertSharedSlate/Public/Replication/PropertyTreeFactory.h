// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/View/Column/IPropertyTreeColumn.h"
#include "Editor/View/Column/ReplicationColumnsUtils.h"
#include "Editor/View/Column/SelectionViewerColumns.h"
#include "Replication/Utils/FilterResult.h"

#include "Delegates/Delegate.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertSharedSlate
{
	class FPropertyData;
	class IPropertyTreeView;
	
	DECLARE_DELEGATE_RetVal_OneParam(EFilterResult, FFilterPropertyData, const FPropertyData&);
	
	struct FCreatePropertyTreeViewParams
	{
		/** Optional. Additional property columns you want added. */
		TArray<FPropertyColumnEntry> PropertyColumns
		{
			ReplicationColumns::Property::LabelColumn(),
			ReplicationColumns::Property::TypeColumn()
		};

		/** Optional filter function. Return true to al */
		FFilterPropertyData FilterItem;
		
		/** Optional initial primary sort mode for object rows */
		FColumnSortInfo PrimaryPropertySort { ReplicationColumns::Property::LabelColumnId, EColumnSortMode::Ascending };
		/** Optional initial secondary sort mode for object rows */
		FColumnSortInfo SecondaryPropertySort { ReplicationColumns::Property::LabelColumnId, EColumnSortMode::Ascending };
		
		/** Optional widget to add to the left of the property list search bar. */
		TAlwaysValidWidget LeftOfPropertySearchBar;
		/** Optional widget to add to the right of the property list search bar. */
		TAlwaysValidWidget RightOfPropertySearchBar;
		/** Optional widget to add between the search bar and the table view (e.g. a SBasicFilterBar). */
		TAlwaysValidWidget RowBelowSearchBar;
		/** Optional, alternate content to show instead of the tree view when there are no rows. */
		TAlwaysValidWidget NoItemsContent;
	};
	
	/**
	 * Creates a tree view that uses a search box for filtering items.
	 * You can customize this tree view by adding custom widgets and columns into the property view.
	 */
	CONCERTSHAREDSLATE_API TSharedRef<IPropertyTreeView> CreateSearchablePropertyTreeView(FCreatePropertyTreeViewParams Params = {});
}
