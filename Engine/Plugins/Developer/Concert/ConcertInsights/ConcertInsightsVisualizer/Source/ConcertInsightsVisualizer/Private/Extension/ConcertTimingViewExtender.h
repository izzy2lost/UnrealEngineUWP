// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertTimingViewSession.h"

#include "Insights/ITimingViewExtender.h"
#include "Templates/UniquePtr.h"

namespace UE::ConcertInsightsVisualizer
{
	/** Keeps track of FConcertTimingViewSession per analytics session. */
	class FConcertTimingViewExtender : public Insights::ITimingViewExtender
	{
	public:

		//~ Begin Insights::ITimingViewExtender Interface
		virtual void OnBeginSession(Insights::ITimingViewSession& InSession) override;
		virtual void OnEndSession(Insights::ITimingViewSession& InSession) override;
		virtual void Tick(Insights::ITimingViewSession& InTimingSession, const TraceServices::IAnalysisSession& InAnalysisSession) override;
		virtual void ExtendFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder) override;
		//~ End Insights::ITimingViewExtender Interface

	private:

		struct FPerSessionData
		{
			TUniquePtr<FConcertTimingViewSession> SharedData;
		};

		//** The data we host per-session */
		TMap<Insights::ITimingViewSession*, FPerSessionData> PerSessionDataMap;
	};
}

