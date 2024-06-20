// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

namespace UE::Insights { class FTableColumn; }

// Column identifiers
struct FStatsViewColumns
{
	static const FName NameColumnID;
	static const FName MetaGroupNameColumnID;
	static const FName TypeColumnID;
	static const FName DataTypeColumnID;
	static const FName CountColumnID;
	static const FName SumColumnID;
	static const FName MaxColumnID;
	static const FName UpperQuartileColumnID;
	static const FName AverageColumnID;
	static const FName MedianColumnID;
	static const FName LowerQuartileColumnID;
	static const FName MinColumnID;
	static const FName DiffColumnID;
};

struct FStatsViewColumnFactory
{
public:
	static void CreateStatsViewColumns(TArray<TSharedRef<UE::Insights::FTableColumn>>& Columns);

	static TSharedRef<UE::Insights::FTableColumn> CreateNameColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMetaGroupNameColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateTypeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateDataTypeColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateCountColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateSumColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMaxColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateUpperQuartileColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateAverageColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMedianColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateLowerQuartileColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateMinColumn();
	static TSharedRef<UE::Insights::FTableColumn> CreateDiffColumn();

private:
	static constexpr float AggregatedStatsColumnInitialWidth = 80.0f;
};
