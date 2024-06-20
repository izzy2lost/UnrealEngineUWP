// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TraceInsightsCore
#include "InsightsCore/Table/ViewModels/TableColumn.h"
#include "InsightsCore/Table/ViewModels/TableCellValueSorter.h"
#include "InsightsCore/Table/ViewModels/TreeNodeGrouping.h"

// TraceInsights
#include "Insights/ViewModels/StatsNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sorters
////////////////////////////////////////////////////////////////////////////////////////////////////

class FStatsNodeSortingByStatsType : public UE::Insights::FTableCellValueSorter
{
public:
	FStatsNodeSortingByStatsType(TSharedRef<UE::Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<UE::Insights::FBaseTreeNodePtr>& NodesToSort, UE::Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FStatsNodeSortingByDataType : public UE::Insights::FTableCellValueSorter
{
public:
	FStatsNodeSortingByDataType(TSharedRef<UE::Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<UE::Insights::FBaseTreeNodePtr>& NodesToSort, UE::Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FStatsNodeSortingByCount : public UE::Insights::FTableCellValueSorter
{
public:
	FStatsNodeSortingByCount(TSharedRef<UE::Insights::FTableColumn> InColumnRef);

	virtual void Sort(TArray<UE::Insights::FBaseTreeNodePtr>& NodesToSort, UE::Insights::ESortMode SortMode) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Organizers
////////////////////////////////////////////////////////////////////////////////////////////////////

/** Enumerates types of grouping or sorting for the stats nodes. */
enum class EStatsGroupingMode
{
	/** Creates a single group for all timers. */
	Flat,

	/** Creates one group for one letter. */
	ByName,

	/** Creates groups based on stats metadata group names. */
	ByMetaGroupName,

	/** Creates one group for each node type. */
	ByType,

	/** Creates one group for each data type. */
	ByDataType,

	/** Creates one group for each logarithmic range ie. 0, [1 .. 10), [10 .. 100), [100 .. 1K), etc. */
	ByCount,

	/** Invalid enum type, may be used as a number of enumerations. */
	InvalidOrMax,
};

/** Type definition for shared pointers to instances of EStatsGroupingMode. */
typedef TSharedPtr<EStatsGroupingMode> EStatsGroupingModePtr;

////////////////////////////////////////////////////////////////////////////////////////////////////
