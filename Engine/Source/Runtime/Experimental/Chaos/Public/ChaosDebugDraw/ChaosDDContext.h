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

		static FChaosDDFramePtr ExtractGlobalFrame();

	private:
		friend class FChaosDDScene;
		friend class FChaosDDTaskContext;
		friend class FChaosDDTaskParentContext;
		friend class FChaosDDTimelineContext;

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
	// Initializes the FChaosDDContext for a thread that owns a timeline
	// 
	// This starts a new frame (debug draw buffer) and sets up the FChaosDDContext for this thread.
	// The active context should be accessed via FChaosDDContext::GetWriter(). FChaosDDTimelineContext
	// is not directly used other than to instantiate.
	//
	class CHAOS_API FChaosDDTimelineContext
	{
	public:
		void BeginFrame(const FChaosDDTimelinePtr& InTimeline, double InTime, double InDt);
		void EndFrame();

	private:
		FChaosDDTimelinePtr Timeline;
		FChaosDDFramePtr PreviousFrame;
	};

	//
	// A scoped wrapper for FChaosDDTimelineContext
	//
	class CHAOS_API FChaosDDScopeTimelineContext
	{
	public:
		FChaosDDScopeTimelineContext(const FChaosDDTimelinePtr& InTimeline, double InTime, double InDt);
		~FChaosDDScopeTimelineContext();

	private:
		FChaosDDTimelineContext Context;
	};

	//
	// Used to propagate a debug draw context to a child thread.
	// To use: 
	//		- put a FChaosDDTaskParentContext on the stack on the parent thread
	//		- pass the FChaosDDTaskParentContext to the child thread
	//		- put FChaosDDScopeTaskContext(ParentContext) on the child thread
	// (Search for FChaosDDScopeTaskContext for examples.)
	//
	class CHAOS_API FChaosDDTaskParentContext
	{
	public:
		FChaosDDTaskParentContext();
	private:
		friend class FChaosDDTaskContext;
		FChaosDDFramePtr Frame;
	};

	//
	// Initializes the FChaosDDContext for a task thread.
	// Assumes that the task is kicked off from a thread that has an active debug draw context,
	// which should be passed into this context. Any debug draws from the task will go to the 
	// same frame as the parent context.
	// 
	// The active context is accessed via FChaosDDContext::GetWriter() (and not this object).
	// 
	// NOTE: This is only intended to be used for tasks which will be awaited before the end
	// of the frame (truly asynchronous tasks would need their own timeline, or just set up
	// a context that writes to the global frame)
	// 
	// @todo(chaos): ChaosDDFrame should track how many contexts it is referenced by and
	// assert that it is not active when we end the frame.
	//
	class CHAOS_API FChaosDDTaskContext
	{
	public:
		void BeginThread(const FChaosDDTaskParentContext& InParentDDContext);
		void EndThread();

	private:
		FChaosDDFramePtr PreviousFrame;
	};

	//
	// A scoped wrapper for FChaosDDTaskContext
	//
	class CHAOS_API FChaosDDScopeTaskContext
	{
	public:
		FChaosDDScopeTaskContext(const FChaosDDTaskParentContext& InParentDDContext);
		~FChaosDDScopeTaskContext();

	private:
		FChaosDDTaskContext Context;
	};
}

#endif