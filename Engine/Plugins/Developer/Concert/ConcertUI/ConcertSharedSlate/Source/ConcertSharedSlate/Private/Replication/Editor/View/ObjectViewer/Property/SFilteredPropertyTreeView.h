// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SPropertyTreeView.h"
#include "Replication/Editor/View/Tree/SReplicationTreeView.h"

#include "Filters/SBasicFilterBar.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertSharedSlate
{
	class FReplicatedPropertyData;
	class SReplicationFilterBar;
	
	/** This widget extends SPropertyTreeView with filtering functionality. */
	class SFilteredPropertyTreeView : public SCompoundWidget
	{
	public:
		
		using FFilterRef = TSharedRef<FFilterBase<TSharedPtr<FReplicatedPropertyData>>>;
		
		SLATE_BEGIN_ARGS(SFilteredPropertyTreeView)
		{}
			/*************** Arguments inherited by SReplicationTreeView ***************/
		
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
		
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		
		/** Rebuilds all property data from the property source */
		void RefreshPropertyData(const TSet<FConcertPropertyChain>& PropertiesToDisplay, const FSoftClassPath& Class, bool bCanReusePropertyData) const
		{
			ReplicatedProperties->RefreshPropertyData(PropertiesToDisplay, Class, bCanReusePropertyData);
		}
		
		/** Called when the items need to be refiltered due to the item source changing. */
		void RequestRefilter() const { ReplicatedProperties->RequestRefilter(); }
		/** Requests that the given column be resorted, if it currently affects the row sorting. */
		void RequestResortForColumn(const FName& ColumnId) const { ReplicatedProperties->RequestResortForColumn(ColumnId); }

	private:

		/** The tree view displaying the replicated properties */
		TSharedPtr<SPropertyTreeView> ReplicatedProperties;
		/** Displays the active filters*/
		TSharedPtr<SReplicationFilterBar> FilterBar;

		struct FBuildFilterBarResult
		{
			TArray<FFilterRef> EnabledByDefault;
			TArray<FFilterRef> DisabledByDefault;
		};
		/** Build the filter bar and returns the filters that should be active by default. */
		FBuildFilterBarResult BuildFilterBar();

		/** Runs all filters through this item */
		bool PassesFilters(const TSharedPtr<FReplicatedPropertyData>& ReplicatedPropertyData) const;
		bool PassesAnyFilters(const TSharedPtr<FReplicatedPropertyData>& ReplicatedPropertyData) const;
	};
}

