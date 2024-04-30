// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosDebugDraw/ChaosDDTypes.h"
#include "Containers/Array.h"
#include "HAL/CriticalSection.h"
#include "Math/Sphere.h"

#if CHAOS_DEBUG_DRAW

namespace ChaosDD::Private
{
	class IChaosDDRenderer;
	class FChaosDDTimeline;

	//
	// Debug draw system for a world. In PIE there will be one of these for the server and each client.
	//
	// @todo(chaos): enable retention of debug draw frames and debug draw from a specific time
	class CHAOS_API FChaosDDScene
	{
	public:
		FChaosDDScene(const FString& InName);
		~FChaosDDScene();

		const FString& GetName() const
		{
			return Name;
		}

		// Specify the region of in which we wish to enable debug draw. A radius of zero means everywhere.
		void SetDrawRegion(const FSphere3d& InDrawRegion);

		// Set the line budget for debug draw
		void SetCommandBudget(int32 InCommandBudget);

		// Create a new timeline. E.g., PT, GT, RBAN
		FChaosDDTimelinePtr CreateTimeline(const FString& Name);
		
		// Release a timeline
		void ReleaseTimeline(const FChaosDDTimelinePtr& Scene);

		// Render the most recent frame from each timeline
		void RenderLatestFrames(IChaosDDRenderer& Renderer);

	private:
		TArray<FChaosDDFramePtr> GetFrames();

		FString Name;
		FCriticalSection TimelinesCS;
		TArray<FChaosDDTimelinePtr> Timelines;
		FSphere3d DrawRegion;
		int32 CommandBudget;
	};
}

#endif