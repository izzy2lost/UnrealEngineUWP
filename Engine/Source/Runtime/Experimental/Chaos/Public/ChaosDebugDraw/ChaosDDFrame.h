// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosDebugDraw/ChaosDDTypes.h"
#include "Chaos/DebugDrawCommand.h"
#include "Containers/Map.h"
#include "HAL/PlatformTLS.h"
#include "HAL/CriticalSection.h"
#include "Math/Box.h"
#include "Math/Sphere.h"


#if CHAOS_DEBUG_DRAW

namespace ChaosDD::Private
{
	//
	// A single frame of debug draw data
	//
	class CHAOS_API FChaosDDFrame : public TSharedFromThis<FChaosDDFrame>
	{
	public:
		FChaosDDFrame(const FChaosDDTimelineWeakPtr& InTimeline, int64 InFrameIndex, double InTime, double InDt, const FSphere3d& InDrawRegion, int32 InCommandBudget, int32 InCommandQueueLength)
			: Timeline(InTimeline)
			, FrameIndex(InFrameIndex)
			, Time(InTime)
			, Dt(InDt)
			, DrawRegion(InDrawRegion)
			, CommandBudget(InCommandBudget)
			, CommandCost(0)
		{
			if (InCommandQueueLength > 0)
			{
				Commands.Reserve(InCommandQueueLength);
			}
		}

		~FChaosDDFrame()
		{
		}

		FChaosDDTimelinePtr GetTimeline() const
		{
			return Timeline.Pin();
		}

		int64 GetFrameIndex() const
		{
			return FrameIndex;
		}

		double GetTime() const
		{
			return Time;
		}

		double GetDt() const
		{
			return Dt;
		}

		bool IsInDrawRegion(const FVector& InPos)
		{
			if (DrawRegion.W > 0.0)
			{
				return DrawRegion.IsInside(InPos);
			}
			return true;
		}

		bool IsInDrawRegion(const FSphere3d& InSphere)
		{
			if (DrawRegion.W > 0.0)
			{
				return DrawRegion.Intersects(InSphere);
			}
			return true;
		}

		bool IsInDrawRegion(const FBox3d& InBox)
		{
			if (DrawRegion.W > 0.0)
			{
				const FBox3d DrawRegionBox = FBox3d(DrawRegion.Center - FVector(DrawRegion.W), DrawRegion.Center + FVector(DrawRegion.W));
				return DrawRegionBox.Intersect(InBox);
			}
			return true;
		}

		bool AddToCost(int32 InCost)
		{
			FScopeLock Lock(&CommandsCS);

			CommandCost += InCost;

			// A budget of zero means infinite
			return ((CommandBudget == 0) || (CommandCost <= CommandBudget));
		}

		void EnqueueCommand(const Chaos::FLatentDrawCommand& InCommand)
		{
			FScopeLock Lock(&CommandsCS);

			Commands.Add(InCommand);
		}

		int32 GetNumCommands() const
		{
			FScopeLock Lock(&CommandsCS);

			return Commands.Num();
		}

		template<typename VisitorType>
		void VisitCommands(const VisitorType& Visitor)
		{
			FScopeLock Lock(&CommandsCS);

			for (const Chaos::FLatentDrawCommand& Command : Commands)
			{
				Visitor(Command);
			}
		}

	private:
		FChaosDDTimelineWeakPtr Timeline;
		int64 FrameIndex;
		double Time;
		double Dt;
		const FSphere3d& DrawRegion;
		int32 CommandBudget;
		int32 CommandCost;

		// Legacy debug draw commands in a lock-protected queue
		// @todo(chaos): we could move these to the per-thread buffer
		TArray<Chaos::FLatentDrawCommand> Commands;
		mutable FCriticalSection CommandsCS;
	};
}

#endif
