// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosDebugDraw/ChaosDDContext.h"
#include "ChaosDebugDraw/ChaosDDLog.h"
#include "ChaosDebugDraw/ChaosDDFrame.h"
#include "ChaosDebugDraw/ChaosDDTimeline.h"

#include "ChaosLog.h"

DEFINE_LOG_CATEGORY(LogChaosDD);

#if CHAOS_DEBUG_DRAW

namespace ChaosDD::Private
{
	FChaosDDContext::FChaosDDContext()
	{
		TimelineStack.Push(FChaosDDTimelinePtr());
	}

	void FChaosDDContext::BeginFrame(const FChaosDDTimelinePtr& InTimeline, double Time, double Dt)
	{
		FChaosDDContext& Context = Get();

		if (InTimeline.IsValid())
		{
			Context.TimelineStack.Push(InTimeline);

			Context.GetTimeline()->BeginFrame(Time, Dt);
		}

		Context.UpdateCachedFramePtr();

	}

	void FChaosDDContext::EndFrame()
	{
		FChaosDDContext& Context = Get();

		if (ensure(Context.GetTimeline().IsValid()))
		{
			Context.GetTimeline()->EndFrame();

			Context.TimelineStack.Pop();
		}

		Context.UpdateCachedFramePtr();
	}

	void FChaosDDContext::UpdateCachedFramePtr()
	{
		if (GetTimeline().IsValid())
		{
			Frame = GetTimeline()->GetActiveFrame();
		}
		else
		{
			Frame.Reset();
		}
	}

}

#endif