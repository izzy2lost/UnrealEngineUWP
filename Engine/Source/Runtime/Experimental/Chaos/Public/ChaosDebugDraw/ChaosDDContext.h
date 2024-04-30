// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosDebugDraw/ChaosDDTypes.h"
#include "HAL/ThreadSingleton.h"

#if CHAOS_DEBUG_DRAW

namespace ChaosDD::Private
{
	//
	// A thread-local debug draw context used to access the queue to draw to
	// for any thread on which debug draw has been set up.
	//
	class CHAOS_API FChaosDDContext : public TThreadSingleton<FChaosDDContext>
	{
	public:
		FChaosDDContext();

		// The frame we should be drawing into on this thread
		const FChaosDDFramePtr& GetFrame() const
		{ 
			return Frame;
		}

		// The timeline that holds the frames from this thread
		const FChaosDDTimelinePtr& GetTimeline() const
		{
			// NOTE: TimelineStack has null as its first element so that we can always return a shared pointer by ref
			return TimelineStack.Top();
		}

		// Allocate a new frame for the timeline and set up the context
		static void BeginFrame(const FChaosDDTimelinePtr& InTimeline, double Time, double Dt);

		// Signal that the frame is complete and can be rendered
		static void EndFrame();

	private:
		void UpdateCachedFramePtr();

		// We need a stack of timelines to handle nested BeginFrame calls. E.g., for sub-steps in the solver.
		TArray<FChaosDDTimelinePtr> TimelineStack;
		FChaosDDFramePtr Frame;
	};
}

#endif