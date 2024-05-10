// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosDebugDraw/ChaosDDScene.h"
#include "ChaosDebugDraw/ChaosDDContext.h"
#include "ChaosDebugDraw/ChaosDDFrame.h"
#include "ChaosDebugDraw/ChaosDDLog.h"
#include "ChaosDebugDraw/ChaosDDRenderer.h"
#include "ChaosDebugDraw/ChaosDDTimeline.h"

#if CHAOS_DEBUG_DRAW

namespace ChaosDD::Private
{
	template<typename LambdaType>
	void VisitTimelines(const TArray<FChaosDDTimelineWeakPtr>& Timelines, const LambdaType& Visitor)
	{
		for (const FChaosDDTimelineWeakPtr& TimelineWeak : Timelines)
		{
			if (const FChaosDDTimelinePtr& Timeline = TimelineWeak.Pin())
			{
				Visitor(Timeline);
			}
		}
	}

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

		VisitTimelines(Timelines,
			[&InDrawRegion](const FChaosDDTimelinePtr& Timeline)
			{
				Timeline->SetDrawRegion(InDrawRegion);
			});

		// The global timeline doesn't use a draw region because it could be shared
		// among multiple viewports and they probably don't want the same region
		//FChaosDDContext::SetGlobalDrawRegion(InDrawRegion);
	}

	void FChaosDDScene::SetCommandBudget(int32 InCommandBudget)
	{
		FScopeLock Lock(&TimelinesCS);

		CommandBudget = InCommandBudget;

		VisitTimelines(Timelines,
			[InCommandBudget](const FChaosDDTimelinePtr& Timeline)
			{
				Timeline->SetCommandBudget(InCommandBudget);
			});

		FChaosDDContext::SetGlobalCommandBudget(InCommandBudget);
	}

	FChaosDDTimelinePtr FChaosDDScene::CreateTimeline(const FString& InName)
	{
		FScopeLock Lock(&TimelinesCS);

		FChaosDDTimelinePtr Timeline = MakeShared<FChaosDDTimeline>(InName, CommandBudget);

		Timelines.Add(Timeline.ToWeakPtr());

		return Timeline;
	}

	void FChaosDDScene::RenderLatestFrames(IChaosDDRenderer& Renderer, bool bIncludeGlobalFrame)
	{
		TArray<FChaosDDFramePtr> Frames = GetFrames();

		if (bIncludeGlobalFrame)
		{
			Frames.Add(FChaosDDContext::ExtractGlobalFrame());
		}

		for (const FChaosDDFramePtr& Frame : Frames)
		{
			if (Frame.IsValid())
			{
				UE_LOG(LogChaosDD, VeryVerbose, TEXT("Render %s %d %d Commands"), *Frame->GetTimeline()->GetName(), Frame->GetFrameIndex(), Frame->GetNumCommands());

				// Render the legacy commands
				Frame->VisitLatentCommands(
					[&Renderer](const Chaos::FLatentDrawCommand& Command)
					{
						Renderer.RenderLatentCommand(Command);
					});

				// Render the commands
				Frame->VisitCommands(
					[&Renderer](const FChaosDDCommand& Command)
					{
						Command(Renderer);
					});
			}
		}
	}

	TArray<FChaosDDFramePtr> FChaosDDScene::GetFrames()
	{
		FScopeLock Lock(&TimelinesCS);

		TArray<FChaosDDFramePtr> Frames;

		VisitTimelines(Timelines,
			[&Frames](const FChaosDDTimelinePtr& Timeline)
			{
				Timeline->GetFrames(Frames);
			});

		return Frames;
	}

	void FChaosDDScene::PruneTimelines()
	{
		FScopeLock Lock(&TimelinesCS);

		// Remove all timelines that have been deleted
		Timelines.RemoveAll(
			[](const FChaosDDTimelineWeakPtr& Timeline)
			{
				return !Timeline.IsValid();
			});
	}
}

#endif
