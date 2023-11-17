// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimerFilters.h"

#include "Insights/TimingProfilerManager.h"
#include "Insights/ViewModels/ThreadTimingTrack.h"

#define LOCTEXT_NAMESPACE "Insights::TimerFilters"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FTimerNameFilterState)
INSIGHTS_IMPLEMENT_RTTI(FTimerNameFilter)

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimerNameFilterState
////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimerNameFilterState::Update()
{
	if (FilterValue.IsEmpty() || !SelectedOperator.IsValid())
	{
		return;
	}

	TimerIds.Empty();
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		uint32 TimerCount = TimerReader->GetTimerCount();
		for (uint32 TimerIndex = 0; TimerIndex < TimerCount; ++TimerIndex)
		{
			const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(TimerIndex);
			if (Timer && Timer->Name)
			{
				if (SelectedOperator->GetKey() == EFilterOperator::Eq)
				{
					if (FCString::Stricmp(Timer->Name, *FilterValue) == 0)
					{
						TimerIds.Add(Timer->Id);
					}
				}
				else if (SelectedOperator->GetKey() == EFilterOperator::Contains)
				{
					if (FCString::Stristr(Timer->Name, *FilterValue))
					{
						TimerIds.Add(Timer->Id);
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FTimerNameFilterState::ApplyFilter(const FFilterContext& Context) const
{
	if (!Context.HasFilterData(static_cast<int32>(Filter->GetKey())))
	{
		return Context.GetReturnValueForUnsetFilters();
	}

	int64 Value;
	Context.GetFilterData<int64>(Filter->GetKey(), Value);

	return TimerIds.Contains(static_cast<uint32>(Value));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimerNameFilter
////////////////////////////////////////////////////////////////////////////////////////////////////

FTimerNameFilter::FTimerNameFilter()
	: FCustomFilter(static_cast<int32>(EFilterField::TimerName),
		LOCTEXT("TimerName", "Timer Name"),
		LOCTEXT("TimerName", "Timer Name"),
		EFilterDataType::Custom,
		nullptr,
		nullptr)
{
	SupportedOperators = MakeShared<TArray<TSharedPtr<IFilterOperator>>>();
	SupportedOperators->Add(StaticCastSharedRef<IFilterOperator>(MakeShared<FFilterOperator<int64>>(EFilterOperator::Eq, TEXT("Is"), [](int64 lhs, int64 rhs) { return lhs == rhs; })));
	SupportedOperators->Add(StaticCastSharedRef<IFilterOperator>(MakeShared<FFilterOperator<int64>>(EFilterOperator::Contains, TEXT("Contains"), [](int64 lhs, int64 rhs) { return lhs == rhs; })));

	SetCallback([this](const FString& Text, TArray<FString>& OutSuggestions)
		{
			this->PopulateTimerNameSuggestionList(Text, OutSuggestions);
		});
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimerNameFilter::PopulateTimerNameSuggestionList(const FString& Text, TArray<FString>& OutSuggestions)
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		uint32 TimerCount = TimerReader->GetTimerCount();
		for (uint32 TimerIndex = 0; TimerIndex < TimerCount; ++TimerIndex)
		{
			const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(TimerIndex);
			if (Timer && Timer->Name)
			{
				if (Text.IsEmpty())
				{
					OutSuggestions.Add(Timer->Name);
					continue;
				}
				const TCHAR* FoundString = FCString::Stristr(Timer->Name, *Text);
				if (FoundString)
				{
					OutSuggestions.Add(Timer->Name);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE