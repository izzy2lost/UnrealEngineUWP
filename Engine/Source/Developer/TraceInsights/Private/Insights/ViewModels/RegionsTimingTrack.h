// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

// TraceServices
#include "TraceServices/Model/Regions.h"

// TraceInsights
#include "Insights/ITimingViewExtender.h"
#include "Insights/ViewModels/TimingEventsTrack.h"

namespace UE::Insights::TimingProfiler { class STimingView; }

namespace Insights
{

class FTimingRegionsTrack;

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimingRegionsViewCommands : public TCommands<FTimingRegionsViewCommands>
{
public:
	FTimingRegionsViewCommands();
	virtual ~FTimingRegionsViewCommands() override;
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> ShowHideRegionTrack;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimingRegionsSharedState : public UE::Insights::Timing::ITimingViewExtender, public TSharedFromThis<FTimingRegionsSharedState>
{
	friend class FTimingRegionsTrack;
	
public:
	explicit FTimingRegionsSharedState(UE::Insights::TimingProfiler::STimingView* InTimingView) : TimingView(InTimingView) {}
	virtual ~FTimingRegionsSharedState() override = default;

	//////////////////////////////////////////////////
	// ITimingViewExtender interface

	virtual void OnBeginSession(UE::Insights::Timing::ITimingViewSession& InSession) override;
	virtual void OnEndSession(UE::Insights::Timing::ITimingViewSession& InSession) override;
	virtual void Tick(UE::Insights::Timing::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession) override;
	void ShowHideRegionsTrack();
	bool IsRegionsTrackVisible() const {return bShowHideRegionsTrack;};
	virtual void ExtendOtherTracksFilterMenu(UE::Insights::Timing::ITimingViewSession& InSession, FMenuBuilder& InOutMenuBuilder) override;
	void BindCommands();

private:
	UE::Insights::TimingProfiler::STimingView* TimingView;
	TSharedPtr<FTimingRegionsTrack> TimingRegionsTrack;

	bool bShowHideRegionsTrack = true;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimingRegionsTrack : public FTimingEventsTrack
{
	INSIGHTS_DECLARE_RTTI(FTimingRegionsTrack, FTimingEventsTrack)

public:
	FTimingRegionsTrack(FTimingRegionsSharedState& InSharedState) : FTimingEventsTrack(TEXT("Timing Regions")), SharedState(InSharedState) {}
	virtual ~FTimingRegionsTrack() override;

	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual void InitTooltip(FTooltipDrawState& InOutTooltip, const ITimingEvent& InTooltipEvent) const override;
	virtual const TSharedPtr<const ITimingEvent> SearchEvent(const FTimingEventSearchParameters& InSearchParameters) const override;
	virtual void BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context) override;
	virtual void BuildFilteredDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context) override;
	virtual void SetFilterConfigurator(TSharedPtr<UE::Insights::FFilterConfigurator> InFilterConfigurator) override;
	virtual bool HasCustomFilter() const override;
	virtual void OnClipboardCopyEvent(const ITimingEvent& InSelectedEvent) const override;

protected:
	bool FindRegionEvent(const FTimingEventSearchParameters& InParameters, TFunctionRef<void(double, double, uint32, const TraceServices::FTimeRegion&)> InFoundPredicate) const;

private:
	TSharedPtr<UE::Insights::FFilterConfigurator> FilterConfigurator;
	FTimingRegionsSharedState& SharedState;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
