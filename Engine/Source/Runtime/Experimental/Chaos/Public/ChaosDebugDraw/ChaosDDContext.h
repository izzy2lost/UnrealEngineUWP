// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosDebugDraw/ChaosDDFrame.h"
#include "ChaosDebugDraw/ChaosDDTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/ThreadSingleton.h"
#include "Misc/ScopeLock.h"

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

		static FChaosDDFrameWriter GetWriter()
		{
			return FChaosDDFrameWriter(Get().GetFrame());
		}

	private:
		friend class FChaosDDScene;
		friend class FChaosDDScopeTimelineContext;
		friend class FChaosDDScopeTaskContext;

		const FChaosDDFramePtr& GetFrame() const
		{
			// The frame we should be drawing into on this thread
			if (!Frame.IsValid())
			{
				// If there is no Context set up on this thread we fall back to a global frame that 
				// is tied to the game thread. If debug draw commands are queued while the game thread
				// is rendering the DDScene, the commands will be split across frames resulting in flicker
				// @todo(chaos): ideally we don't have a global frame - try to get rid of it
				return GetGlobalFrame();
			}

			return Frame;
		}

		// Global frame management
		static const FChaosDDFramePtr& GetGlobalFrame();
		static void CreateGlobalFrame();
		static FChaosDDFramePtr ExtractGlobalFrame();
		static void SetGlobalDrawRegion(const FSphere3d& InDrawRegion);
		static void SetGlobalCommandBudget(int32 InCommandBudget);

		// The frame to draw to on this thread (or null)
		FChaosDDFramePtr Frame;

		// Global frame: fallback for out-of-context debug draw
		static FCriticalSection GlobalFrameCS;
		static FChaosDDFramePtr GlobalFrame;
		static int32 GlobalCommandBudget;
	};

	//
	// A scoped Debug Draw Context for use on the thread that owns the timeline
	//
	class CHAOS_API FChaosDDScopeTimelineContext
	{
	public:
		FChaosDDScopeTimelineContext(const FChaosDDTimelinePtr& InTimeline, double InTime, double InDt);
		~FChaosDDScopeTimelineContext();

	private:
		FChaosDDTimelinePtr Timeline;
		FChaosDDFramePtr ParentFrame;
	};

	//
	// A scoped Debug Draw Context for use in tasks and parallel-for etc.
	// Assumes that the task is kicked off from a thread that has an active debug draw context,
	// which should be passed into this context.
	//
	class CHAOS_API FChaosDDScopeTaskContext
	{
	public:
		FChaosDDScopeTaskContext(const FChaosDDContext& InParentDDContext);
		~FChaosDDScopeTaskContext();

	private:
		FChaosDDFramePtr ParentFrame;
	};
}

#endif