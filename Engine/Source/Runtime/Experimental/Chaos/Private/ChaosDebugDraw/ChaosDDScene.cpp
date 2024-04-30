// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosDebugDraw/ChaosDDScene.h"
#include "ChaosDebugDraw/ChaosDDFrame.h"
#include "ChaosDebugDraw/ChaosDDLog.h"
#include "ChaosDebugDraw/ChaosDDRenderer.h"
#include "ChaosDebugDraw/ChaosDDTimeline.h"

#if CHAOS_DEBUG_DRAW

namespace ChaosDD::Private
{
	FChaosDDScene::FChaosDDScene(const FString& InName)
		: Name(InName)
		, DrawRegion(FVector::Zero(), 0.0)
		, CommandBudget(20000)
	{
	}

	FChaosDDScene::~FChaosDDScene()
	{
	}

	void FChaosDDScene::SetDrawRegion(const FSphere3d& InDrawRegion)
	{
		FScopeLock Lock(&TimelinesCS);

		DrawRegion = InDrawRegion;

		for (const FChaosDDTimelinePtr& Timeline : Timelines)
		{
			Timeline->SetDrawRegion(InDrawRegion);
		}
	}

	void FChaosDDScene::SetCommandBudget(int32 InCommandBudget)
	{
		FScopeLock Lock(&TimelinesCS);

		CommandBudget = InCommandBudget;

		for (const FChaosDDTimelinePtr& Timeline : Timelines)
		{
			Timeline->SetCommandBudget(InCommandBudget);
		}
	}

	FChaosDDTimelinePtr FChaosDDScene::CreateTimeline(const FString& InName)
	{
		FScopeLock Lock(&TimelinesCS);

		FChaosDDTimelinePtr Timeline = MakeShared<FChaosDDTimeline>(InName, CommandBudget);

		Timelines.Add(Timeline);

		return Timeline;
	}

	void FChaosDDScene::ReleaseTimeline(const FChaosDDTimelinePtr& InTimeline)
	{
		FScopeLock Lock(&TimelinesCS);

		for (int32 TimelineIndex = 0; TimelineIndex < Timelines.Num(); ++TimelineIndex)
		{
			if (Timelines[TimelineIndex].Get() == InTimeline.Get())
			{
				Timelines.RemoveAt(TimelineIndex, 1, EAllowShrinking::No);
			}
		}
	}

	void FChaosDDScene::RenderLatestFrames(IChaosDDRenderer& Renderer)
	{
		TArray<FChaosDDFramePtr> Frames = GetFrames();
		for (const FChaosDDFramePtr& Frame : Frames)
		{
			UE_LOG(LogChaosDD, VeryVerbose, TEXT("Render %s %d %d Commands"), *Frame->GetTimeline()->GetName(), Frame->GetFrameIndex(), Frame->GetNumCommands());

			// Render the legacy commands
			Frame->VisitCommands(
				[&Renderer](const Chaos::FLatentDrawCommand& Command)
				{
					Renderer.DrawCommand(Command);
				});
		}
	}

	TArray<FChaosDDFramePtr> FChaosDDScene::GetFrames()
	{
		FScopeLock Lock(&TimelinesCS);

		TArray<FChaosDDFramePtr> Frames;
		for (const FChaosDDTimelinePtr& Timeline : Timelines)
		{
			Timeline->GetFrames(Frames);
		}
		return Frames;
	}
}

#endif
