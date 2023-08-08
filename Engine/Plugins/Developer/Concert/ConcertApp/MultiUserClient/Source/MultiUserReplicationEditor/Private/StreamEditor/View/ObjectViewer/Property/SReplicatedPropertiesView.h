// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/SBasicFilterBar.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "StreamEditor/View/ObjectViewer/SReplicationTreeView.h"

namespace UE::MultiUserReplicationEditor
{
	class FReplicatedPropertyData;
	
	/** Displays a searchable tree view of properties (SReplicationTreeView) and decorates it with a SBasicFilterBar. */
	class SReplicatedPropertiesView : public SCompoundWidget
	{
		using FFilterRef = TSharedRef<FFilterBase<TSharedPtr<FReplicatedPropertyData>>>;
	public:
		
		SLATE_BEGIN_ARGS(SReplicatedPropertiesView)
		{}
			/*************** Arguments inherited by SReplicationTreeView ***************/
		
			/** The items to display */
			SLATE_ARGUMENT(TArray<TSharedPtr<FReplicatedPropertyData>>*, RootItemsSource)
		
			/** Gets an items children for the tree view */
			SLATE_EVENT(SReplicationTreeView<TSharedPtr<FReplicatedPropertyData>>::FGetItemChildren, OnGetChildren)
		
			/** The columns this list should have */
			SLATE_ARGUMENT(TArray<TReplicationColumn<TSharedPtr<FReplicatedPropertyData>>>, Columns)
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)
		
			/** How many items are to allowed to be selected */
			SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)
		
			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfSearchBar)

			/*************** Own arguments ***************/
			// Please add new arguments here in the future
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/** Called when the items need to be refiltered due to the item source changing. */
		void OnItemsChanged() const;

	private:

		/** The tree view displaying the replicated properties */
		TSharedPtr<SReplicationTreeView<TSharedPtr<FReplicatedPropertyData>>> ReplicatedProperties;
		/** Displays the active filters*/
		TSharedPtr<SBasicFilterBar<TSharedPtr<FReplicatedPropertyData>>> FilterBar;

		struct FBuildFilterBarResult
		{
			TArray<FFilterRef> EnabledByDefault;
			TArray<FFilterRef> DisabledByDefault;
		};
		/** Build the filter bar and returns the filters that should be active by default. */
		FBuildFilterBarResult BuildFilterBar();

		/** Runs all filters through this item */
		bool PassesFilters(const TSharedPtr<FReplicatedPropertyData>& ReplicatedPropertyData) const;
	};
}

