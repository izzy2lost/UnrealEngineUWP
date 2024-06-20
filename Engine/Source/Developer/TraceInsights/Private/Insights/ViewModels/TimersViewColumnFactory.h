// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

// TraceInsightsCore
#include "InsightsCore/Table/ViewModels/TableColumn.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

// Column identifiers
struct FTimersViewColumns
{
	static const FName NameColumnID;
	static const FName MetaGroupNameColumnID;
	static const FName TypeColumnID;
	static const FName InstanceCountColumnID;
	static const FName ChildInstanceCountColumnID;

	// Inclusive Time columns
	static const FName TotalInclusiveTimeColumnID;
	static const FName MaxInclusiveTimeColumnID;
	static const FName UpperQuartileInclusiveTimeColumnID;
	static const FName AverageInclusiveTimeColumnID;
	static const FName MedianInclusiveTimeColumnID;
	static const FName LowerQuartileInclusiveTimeColumnID;
	static const FName MinInclusiveTimeColumnID;

	// Exclusive Time columns
	static const FName TotalExclusiveTimeColumnID;
	static const FName MaxExclusiveTimeColumnID;
	static const FName UpperQuartileExclusiveTimeColumnID;
	static const FName AverageExclusiveTimeColumnID;
	static const FName MedianExclusiveTimeColumnID;
	static const FName LowerQuartileExclusiveTimeColumnID;
	static const FName MinExclusiveTimeColumnID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimersTableColumn : public UE::Insights::FTableColumn
{
public:
	FTimersTableColumn(const FName InId)
		: UE::Insights::FTableColumn(InId)
	{}

	FText GetDescription(ETraceFrameType InAggregationMode) const
	{
		switch (InAggregationMode)
		{
		case TraceFrameType_Game:
			return GameFrame_Description;
			break;
		case TraceFrameType_Rendering:
			return RenderingFrame_Description;
			break;
		default:
			return FTableColumn::GetDescription();
		}
	}

	void SetDescription(ETraceFrameType InAggregationMode, FText InDescription)
	{
		switch (InAggregationMode)
		{
		case TraceFrameType_Game:
			GameFrame_Description = InDescription;
			break;
		case TraceFrameType_Rendering:
			RenderingFrame_Description = InDescription;
			break;
		case TraceFrameType_Count:
			UE::Insights::FTableColumn::SetDescription(InDescription);
			break;
		default:
			ensure(0);
		}
	}

private:
	FText GameFrame_Description;
	FText RenderingFrame_Description;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FTimersViewColumnFactory
{
public:
	static void CreateTimersViewColumns(TArray<TSharedRef<UE::Insights::FTableColumn>>& Columns);
	static void CreateTimerTreeViewColumns(TArray<TSharedRef<UE::Insights::FTableColumn>>& Columns);

	static TSharedRef<UE::Insights::FTableColumn> CreateNameColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMetaGroupNameColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateTypeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateInstanceCountColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateChildInstanceCountColumn();

	static TSharedRef<UE::Insights::FTableColumn> CreateTotalInclusiveTimeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMaxInclusiveTimeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateAverageInclusiveTimeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMedianInclusiveTimeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMinInclusiveTimeColumn();

	static TSharedRef<UE::Insights::FTableColumn> CreateTotalExclusiveTimeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMaxExclusiveTimeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateAverageExclusiveTimeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMedianExclusiveTimeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMinExclusiveTimeColumn();

private:
	static constexpr float TotalTimeColumnInitialWidth = 60.0f;
	static constexpr float TimeMsColumnInitialWidth = 50.0f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
