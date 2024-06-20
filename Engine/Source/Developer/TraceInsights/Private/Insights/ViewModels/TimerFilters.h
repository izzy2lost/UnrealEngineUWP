// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Types/SlateEnums.h"
#include "Widgets/Input/SComboBox.h"

// TraceInsightsCore
#include "InsightsCore/Common/SimpleRtti.h"
#include "InsightsCore/Filter/ViewModels/Filters.h"

namespace TraceServices
{
	class ITimingProfilerTimerReader;
}

namespace Insights
{

class FTimerNameFilterState : public UE::Insights::FFilterState
{
	INSIGHTS_DECLARE_RTTI(FTimerNameFilterState, UE::Insights::FFilterState)

public:
	FTimerNameFilterState(TSharedRef<UE::Insights::FFilter> InFilter)
		: FFilterState(InFilter)
	{}

	FTimerNameFilterState(const FTimerNameFilterState& Other)
		: FFilterState(Other)
	{
		FilterValue = Other.FilterValue;
	}

	virtual ~FTimerNameFilterState() {}

	virtual void Update() override;

	virtual bool ApplyFilter(const UE::Insights::FFilterContext& Context) const override;

	virtual void SetFilterValue(FString InFilterValue) override { FilterValue = InFilterValue; }

	virtual bool Equals(const FFilterState& Other) const;
	virtual TSharedRef<FFilterState> DeepCopy() const;

private:
	FString FilterValue;
	TSet<uint32> TimerIds;
};

class FTimerNameFilter : public UE::Insights::FCustomFilter
{
	INSIGHTS_DECLARE_RTTI(FTimerNameFilter, UE::Insights::FCustomFilter)

public:
	FTimerNameFilter();

	virtual ~FTimerNameFilter() {}

	virtual TSharedRef<UE::Insights::FFilterState> BuildFilterState()
	{ 
		return MakeShared<FTimerNameFilterState>(SharedThis(this)); 
	}

	virtual TSharedRef<UE::Insights::FFilterState> BuildFilterState(const UE::Insights::FFilterState& Other)
	{
		return MakeShared<FTimerNameFilterState>(static_cast<const FTimerNameFilterState&>(Other));
	}

	void PopulateTimerNameSuggestionList(const FString& Text, TArray<FString>& OutSuggestions);
};

enum class EMetadataFilterDataType
{
	Bool = 1,
	Int = 2,
	Double = 3,
	String = 4,
};

struct FMetadataFilterDataTypeEntry
{
	FMetadataFilterDataTypeEntry(EMetadataFilterDataType InType, FText InName)
	{
		Type = InType;
		Name = InName;
	}

	EMetadataFilterDataType Type;
	FText Name;
};

class FMetadataFilterState : public UE::Insights::FFilterState, public TSharedFromThis<FMetadataFilterState>
{
	INSIGHTS_DECLARE_RTTI(FMetadataFilterState, UE::Insights::FFilterState)

public:
	FMetadataFilterState(TSharedRef<UE::Insights::FFilter> InFilter);

	virtual ~FMetadataFilterState() {}

	virtual void Update() override;

	virtual bool ApplyFilter(const UE::Insights::FFilterContext& Context) const override;

	virtual void SetFilterValue(FString InFilterValue) override {}

	virtual bool HasCustomUI() const override { return true; }
	virtual void AddCustomUI(TSharedRef<SHorizontalBox> LeftBox) override;

	virtual bool Equals(const FFilterState& Other) const override ;
	virtual TSharedRef<FFilterState> DeepCopy() const override ;

private:
	FText GetKeyTextBoxValue() const;
	void OnKeyTextBoxValueCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

	TSharedRef<SWidget> DataType_OnGenerateWidget(TSharedPtr<FMetadataFilterDataTypeEntry> InDataType);
	void DataType_OnSelectionChanged(TSharedPtr<FMetadataFilterDataTypeEntry> InDataType, ESelectInfo::Type SelectInfo);
	FText DataType_GetSelectionText() const;

	TSharedRef<SWidget> AvailableOperators_OnGenerateWidget(TSharedPtr<UE::Insights::IFilterOperator> InOperator);
	void AvailableOperators_OnSelectionChanged(TSharedPtr<UE::Insights::IFilterOperator> InOperator, ESelectInfo::Type SelectInfo);
	FText AvailableOperators_GetSelectionText() const;

	FText GetTermTextBoxValue() const;
	void OnTermTextBoxValueCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

	bool ApplyFilterToMetadata(TArrayView<const uint8>& Metadata) const;

private:
	FString Key;
	FString Term;

	typedef TVariant<double, int64, bool> ConvertedDataVariant;
	ConvertedDataVariant ConvertedData;

	bool bShowAllMetadataEvents = false;
	
	TArray<TSharedPtr<FMetadataFilterDataTypeEntry>> AvailableDataTypes;
	TSharedPtr<FMetadataFilterDataTypeEntry> SelectedDataType;
	
	TArray<TSharedPtr<UE::Insights::IFilterOperator>> AvailableOperators;
	TArray<TSharedPtr<UE::Insights::IFilterOperator>> BoolOperators;

	TSharedPtr<SComboBox<TSharedPtr<UE::Insights::IFilterOperator>>> OperatorComboBox;

	const TraceServices::ITimingProfilerTimerReader* TimerReader;
};

class FMetadataFilter : public UE::Insights::FFilter
{
	INSIGHTS_DECLARE_RTTI(FMetadataFilter, UE::Insights::FFilter)

public:
	FMetadataFilter();

	virtual ~FMetadataFilter() {}

	virtual TSharedRef<UE::Insights::FFilterState> BuildFilterState()
	{
		return MakeShared<FMetadataFilterState>(SharedThis(this));
	}

	virtual TSharedRef<UE::Insights::FFilterState> BuildFilterState(const UE::Insights::FFilterState& Other)
	{
		return MakeShared<FMetadataFilterState>(static_cast<const FMetadataFilterState&>(Other));
	}
};

} // namespace Insights
