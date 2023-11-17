// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Insights/Common/SimpleRtti.h"
#include "Insights/ViewModels/Filters.h"

namespace Insights
{

class FTimerNameFilterState : public FFilterState
{

	INSIGHTS_DECLARE_RTTI(FTimerNameFilterState, FFilterState)
public:
	FTimerNameFilterState(TSharedRef<FFilter> InFilter)
		: FFilterState(InFilter)
	{}

	virtual ~FTimerNameFilterState() {}

	virtual void Update() override;

	virtual bool ApplyFilter(const FFilterContext& Context) const override;

	virtual void SetFilterValue(FString InFilterValue) override { FilterValue = InFilterValue; }

private:
	FString FilterValue;
	TSet<uint32> TimerIds;
	bool bShouldUpdate = true;
};

class FTimerNameFilter : public FCustomFilter
{
	INSIGHTS_DECLARE_RTTI(FTimerNameFilter, FCustomFilter)

public:
	FTimerNameFilter();

	virtual ~FTimerNameFilter() {}

	virtual TSharedRef<FFilterState> BuildFilterState() 
	{ 
		return MakeShared<FTimerNameFilterState>(SharedThis(this)); 
	}

	virtual TSharedRef<FFilterState> BuildFilterState(const FFilterState& Other)
	{
		return MakeShared<FTimerNameFilterState>(static_cast<const FTimerNameFilterState&>(Other));
	}

	void PopulateTimerNameSuggestionList(const FString& Text, TArray<FString>& OutSuggestions);
};

} // namespace Insights
