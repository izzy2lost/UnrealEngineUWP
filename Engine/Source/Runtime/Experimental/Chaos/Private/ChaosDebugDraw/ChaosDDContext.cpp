// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosDebugDraw/ChaosDDContext.h"
#include "ChaosDebugDraw/ChaosDDLog.h"
#include "ChaosDebugDraw/ChaosDDFrame.h"
#include "ChaosDebugDraw/ChaosDDTimeline.h"
#include "ChaosLog.h"
#include "HAL/IConsoleManager.h"


DEFINE_LOG_CATEGORY(LogChaosDD);

#if CHAOS_DEBUG_DRAW

namespace ChaosDD::Private
{
	bool bChaosDebugDraw_EnableGlobalQueue = true;
	FAutoConsoleVariableRef CVarChaos_DebugDraw_EnableGlobalQueue(TEXT("p.Chaos.DebugDraw.EnableGlobalQueue"), bChaosDebugDraw_EnableGlobalQueue, TEXT(""));

	FCriticalSection FChaosDDContext::GlobalFrameCS;
	FChaosDDFramePtr FChaosDDContext::GlobalFrame;
	int32 FChaosDDContext::GlobalCommandBudget = 20000;

	//
	//
	// Timeline Context
	//
	//

	void FChaosDDTimelineContext::BeginFrame(const FChaosDDTimelinePtr& InTimeline, double InTime, double InDt)
	{
		FChaosDDContext& Context = FChaosDDContext::Get();
		ParentFrame = Context.Frame;

		if (InTimeline.IsValid())
		{
			Timeline = InTimeline;
			Timeline->BeginFrame(InTime, InDt);
			Context.Frame = Timeline->GetActiveFrame();
		}
		else
		{
			Context.Frame.Reset();
		}
	}

	void FChaosDDTimelineContext::EndFrame()
	{
		if (Timeline.IsValid())
		{
			Timeline->EndFrame();
			Timeline.Reset();
		}

		FChaosDDContext& Context = FChaosDDContext::Get();
		Context.Frame = ParentFrame;
		ParentFrame.Reset();
	}

	FChaosDDScopeTimelineContext::FChaosDDScopeTimelineContext(const FChaosDDTimelinePtr& InTimeline, double InTime, double InDt)
	{
		Context.BeginFrame(InTimeline, InTime, InDt);
	}

	FChaosDDScopeTimelineContext::~FChaosDDScopeTimelineContext()
	{
		Context.EndFrame();
	}

	//
	//
	// Task Context
	//
	//

	void FChaosDDTaskContext::BeginThread(const FChaosDDContext& InParentDDContext)
	{
		FChaosDDContext& Context = FChaosDDContext::Get();
		ParentFrame = Context.Frame;

		if (InParentDDContext.Frame.IsValid())
		{
			Context.Frame = InParentDDContext.Frame;
		}
		else
		{
			Context.Frame.Reset();
		}
	}

	void FChaosDDTaskContext::EndThread()
	{
		FChaosDDContext& Context = FChaosDDContext::Get();
		Context.Frame = ParentFrame;
		ParentFrame.Reset();
	}

	FChaosDDScopeTaskContext::FChaosDDScopeTaskContext(const FChaosDDContext& InParentDDContext)
	{
		Context.BeginThread(InParentDDContext);
	}

	FChaosDDScopeTaskContext::~FChaosDDScopeTaskContext()
	{
		Context.EndThread();
	}

	//
	//
	// Thread Local Context
	//
	//

	FChaosDDContext::FChaosDDContext()
	{
	}

	const FChaosDDFramePtr& FChaosDDContext::GetGlobalFrame()
	{
		FScopeLock Lock(&GlobalFrameCS);

		if (!GlobalFrame.IsValid())
		{
			CreateGlobalFrame();
		}

		return GlobalFrame;
	}

	void FChaosDDContext::CreateGlobalFrame()
	{
		if (bChaosDebugDraw_EnableGlobalQueue)
		{
			FScopeLock Lock(&GlobalFrameCS);

			GlobalFrame = MakeShared<FChaosDDGlobalFrame>(GlobalCommandBudget);
		}
	}

	FChaosDDFramePtr FChaosDDContext::ExtractGlobalFrame()
	{
		FScopeLock Lock(&GlobalFrameCS);

		// Handle toggling the cvar in the runtime
		if (!bChaosDebugDraw_EnableGlobalQueue && GlobalFrame.IsValid())
		{
			GlobalFrame.Reset();
		}

		if (GlobalFrame.IsValid())
		{
			return GlobalFrame->ExtractFrame();
		}

		return {};
	}

	void FChaosDDContext::SetGlobalDrawRegion(const FSphere3d& InDrawRegion)
	{
		FScopeLock Lock(&GlobalFrameCS);

		if (GlobalFrame.IsValid())
		{
			GlobalFrame->SetDrawRegion(InDrawRegion);
		}
	}

	void FChaosDDContext::SetGlobalCommandBudget(int32 InCommandBudget)
	{
		FScopeLock Lock(&GlobalFrameCS);

		GlobalCommandBudget = InCommandBudget;

		if (GlobalFrame.IsValid())
		{
			GlobalFrame->SetCommandBudget(InCommandBudget);
		}
	}
}

#endif