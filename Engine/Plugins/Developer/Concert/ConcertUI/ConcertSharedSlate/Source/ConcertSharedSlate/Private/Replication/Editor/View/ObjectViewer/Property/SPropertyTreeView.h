// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"
#include "Replication/Editor/View/Tree/SReplicationTreeView.h"

#include "Filters/SBasicFilterBar.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertSharedSlate
{
	class FReplicatedPropertyData;
	
	/**
	 * This widget knows how to display a list of properties in a tree view.
	 * It generates the items and exposes extension points for more advanced UI, such as filtering.
	 */
	class SPropertyTreeView : public SCompoundWidget
	{
	public:
		
		using FFilterRef = TSharedRef<FFilterBase<TSharedPtr<FReplicatedPropertyData>>>;
		
		SLATE_BEGIN_ARGS(SPropertyTreeView)
		{}
			/*************** Arguments inherited by SReplicationTreeView ***************/
		
			/** Optional callback to do even more filtering of items. */
			SLATE_EVENT(SReplicationTreeView<FReplicatedPropertyData>::FCustomFilter, FilterItem)
		
			/** The columns this list should have */
			SLATE_ARGUMENT(TArray<TReplicationColumn<FReplicatedPropertyData>>, Columns)
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)
			/** Initial primary sort to set. */
			SLATE_ARGUMENT(FColumnSortInfo, PrimarySort)
			/** Initial secondary sort to set. */
			SLATE_ARGUMENT(FColumnSortInfo, SecondarySort)
		
			/** How many items are to allowed to be selected */
			SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)
		
			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfSearchBar)
			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, RightOfSearchBar)
		
			/** Optional widget to add between the search bar and the table view (e.g. a SBasicFilterBar). */
			SLATE_NAMED_SLOT(FArguments, RowBelowSearchBar)
		
			/** Optional, alternate content to show instead of the tree view when there are no rows. */
			SLATE_NAMED_SLOT(FArguments, NoItemsContent)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/**
		 * Rebuilds all property data from the property source.
		 *
		 * @param PropertiesToDisplay The properties to display
		 * @param Class The class from which the PropertiesToDisplay come
		 * @param bCanReuseExistingRowItems True, will try to reuse rows for properties in the tree already (retains selected rows).
		 *	Set this to false, if all rows should be regenerated (clears selection).
		 *	In general, always set this to false if you've changed the object for which you're displaying the class.
		 */
		void RefreshPropertyData(const TSet<FConcertPropertyChain>& PropertiesToDisplay, const FSoftClassPath& Class, bool bCanReuseExistingRowItems);
		
		/** Called when the items need to be refiltered due to the item source changing. */
		void RequestRefilter() const { TreeView->RequestRefilter(); }
		/** Requests that the given column be resorted, if it currently affects the row sorting. */
		void RequestResortForColumn(const FName& ColumnId) { TreeView->RequestResortForColumn(ColumnId); }

	private:

		/** The tree view displaying the replicated properties */
		TSharedPtr<SReplicationTreeView<FReplicatedPropertyData>> TreeView;
		
		/**
		 * These instances can be subclasses of FReplicatedPropertyData, e.g. FReplicatedPropertyData_Editor.
		 * Their type can be overridden by subclasses.
		 * They only have the FReplicatedPropertyData type so they can be passed efficiently to SObjectToPropertyView.
		 * @see GetPropertyData
		 */
		TArray<TSharedPtr<FReplicatedPropertyData>> PropertyRowData;
		/** The instances of ObjectRowData which do not have any parents. This acts as the item source for the tree view. */
		TArray<TSharedPtr<FReplicatedPropertyData>> RootPropertyRowData;
		/** Inverse map of PropertyRowData using FReplicatedPropertyData::GetProperty as key. Contains all elements of PropertyRowData. */
		TMap<FConcertPropertyChain, TSharedPtr<FReplicatedPropertyData>> ChainToPropertyDataCache;
		
		TSharedRef<FReplicatedPropertyData> AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain);

		/** Inits RootPropertyRowData from PropertyRowData. */
		void BuildRootPropertyRowData();
		void GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild);
	};
}

