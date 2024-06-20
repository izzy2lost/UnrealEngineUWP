// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TraceServices
#include "TraceServices/Model/TimingProfiler.h"

// TraceInsights
#include "Insights/ViewModels/StatsAggregator.h"

namespace UE::Insights::TimingProfiler
{

class FTimerButterflyAggregator : public ::Insights::FStatsAggregator
{
public:
	FTimerButterflyAggregator() : ::Insights::FStatsAggregator(TEXT("Butterfly")) {}
	virtual ~FTimerButterflyAggregator() {}

	TraceServices::ITimingProfilerButterfly* GetResultButterfly() const;
	void ResetResults();

protected:
	virtual ::Insights::IStatsAggregationWorker* CreateWorker(TSharedPtr<const TraceServices::IAnalysisSession> InSession) override;
};

} // namespace UE::Insights::TimingProfiler
