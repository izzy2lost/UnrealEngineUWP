// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/IPropertyTreeView.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"

#include "Filters/SBasicFilterBar.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertSharedSlate
{
	class FReplicatedPropertyData;
	enum class EFilterResult : uint8;
}

namespace UE::ConcertClientSharedSlate
{
	class SReplicationFilterBar;
	struct FFilterablePropertyTreeViewParams;
	
	/** This widget extends SPropertyTreeView with filtering functionality. */
	class SFilteredPropertyTreeView
		: public SCompoundWidget
		, public ConcertSharedSlate::IPropertyTreeView
	{
	public:
		
		using FFilterRef = TSharedRef<FFilterBase<const ConcertSharedSlate::FReplicatedPropertyData&>>;
		
		SLATE_BEGIN_ARGS(SFilteredPropertyTreeView)
		{}
			/*************** Arguments inherited by SReplicationTreeView ***************/
		
			/** The columns this list should have */
			SLATE_ARGUMENT(TArray<ConcertSharedSlate::TReplicationColumn<ConcertSharedSlate::FReplicatedPropertyData>>, Columns)
			/** The name of the column that will have the SExpanderArrow for the tree view. */
			SLATE_ARGUMENT(FName, ExpandableColumnLabel)
			/** Initial primary sort to set. */
			SLATE_ARGUMENT(ConcertSharedSlate::FColumnSortInfo, PrimarySort)
			/** Initial secondary sort to set. */
			SLATE_ARGUMENT(ConcertSharedSlate::FColumnSortInfo, SecondarySort)
		
			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfSearchBar)
			/** Optional widget to add to the left of the search bar. */
			SLATE_NAMED_SLOT(FArguments, RightOfSearchBar)
		
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, FFilterablePropertyTreeViewParams Params);
		
		//~ Begin IPropertyTreeView Interface
		virtual void RefreshPropertyData(const TSet<FConcertPropertyChain>& PropertiesToDisplay, const FSoftClassPath& Class, bool bCanReuseExistingRowItems) override
		{
			ExtendedTreeView->RefreshPropertyData(PropertiesToDisplay, Class, bCanReuseExistingRowItems);
		}
		virtual void RequestRefilter() const override { ExtendedTreeView->RequestRefilter(); }
		virtual void RequestResortForColumn(const FName& ColumnId) override { ExtendedTreeView->RequestResortForColumn(ColumnId); }
		virtual TSharedRef<SWidget> GetWidget() override { return SharedThis(this); }
		//~ End IPropertyTreeView Interface

	private:

		/** The tree view displaying the replicated properties */
		TSharedPtr<IPropertyTreeView> ExtendedTreeView;
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
		ConcertSharedSlate::EFilterResult PassesFilters(const ConcertSharedSlate::FReplicatedPropertyData& ReplicatedPropertyData) const;
		bool PassesAnyFilters(const ConcertSharedSlate::FReplicatedPropertyData& ReplicatedPropertyData) const;
	};
}

