// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/SBasicFilterBar.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Replication/Editor/View/Tree/SReplicationTreeView.h"

namespace UE::ConcertClientSharedSlate
{
	class FReplicatedObjectData;
	class FReplicatedPropertyData;
	class SReplicationFilterBar;
	
	/** Displays a searchable tree view of properties (SReplicationTreeView) and decorates it with a SBasicFilterBar. */
	class SPropertyTreeView : public SCompoundWidget
	{
	public:
		
		using FFilterRef = TSharedRef<FFilterBase<TSharedPtr<FReplicatedPropertyData>>>;
		
		SLATE_BEGIN_ARGS(SPropertyTreeView)
		{}
			/*************** Arguments inherited by SReplicationTreeView ***************/
		
			/** The items to display */
			SLATE_ARGUMENT(TArray<TSharedPtr<FReplicatedPropertyData>>*, RootItemsSource)
		
			/** Gets an items children for the tree view */
			SLATE_EVENT(SReplicationTreeView<FReplicatedPropertyData>::FGetItemChildren, OnGetChildren)
		
			/** The columns this list should have */
			SLATE_ARGUMENT(TArray<TReplicationColumn<FReplicatedPropertyData>>, Columns)
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)
		
			/** How many items are to allowed to be selected */
			SLATE_ARGUMENT(ESelectionMode::Type, SelectionMode)
		
			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfSearchBar)

			/*************** Own arguments ***************/
			// Please add new arguments here in the future

			/** Gets the objects being property edited. */
			SLATE_ATTRIBUTE(TArray<FSoftObjectPath>, SelectedObjects)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/** Called when the items need to be refiltered due to the item source changing. */
		void OnItemsChanged() const;

	private:

		/** The tree view displaying the replicated properties */
		TSharedPtr<SReplicationTreeView<FReplicatedPropertyData>> ReplicatedProperties;
		/** Displays the active filters*/
		TSharedPtr<SReplicationFilterBar> FilterBar;

		/** Used to tell the user that the selected object have all properties filtered out. */
		TAttribute<TArray<FSoftObjectPath>> SelectedObjectsAttribute;

		struct FBuildFilterBarResult
		{
			TArray<FFilterRef> EnabledByDefault;
			TArray<FFilterRef> DisabledByDefault;
		};
		/** Build the filter bar and returns the filters that should be active by default. */
		FBuildFilterBarResult BuildFilterBar();

		/** Runs all filters through this item */
		bool PassesFilters(const TSharedPtr<FReplicatedPropertyData>& ReplicatedPropertyData) const;

		/** Gets the message to display when all properties are filtered out. */
		FText GetAllFilteredText() const;
	};
}

