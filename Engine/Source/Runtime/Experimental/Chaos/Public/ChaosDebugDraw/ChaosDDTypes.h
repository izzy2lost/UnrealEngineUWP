// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Declares.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

#if CHAOS_DEBUG_DRAW

// @todo(chaos): Move to ChaosDD when API is decided
namespace ChaosDD::Private
{
	class FChaosDDContext;
	class FChaosDDFrame;
	class FChaosDDScene;
	class FChaosDDTimeline;

	class IChaosDDRenderer;

	using FChaosDDFramePtr = TSharedPtr<FChaosDDFrame, ESPMode::ThreadSafe>;
	using FChaosDDScenePtr = TSharedPtr<FChaosDDScene, ESPMode::ThreadSafe>;
	using FChaosDDTimelinePtr = TSharedPtr<FChaosDDTimeline, ESPMode::ThreadSafe>;
	using FChaosDDTimelineWeakPtr = TWeakPtr<FChaosDDTimeline, ESPMode::ThreadSafe>;
}

#endif