// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "ChaosDebugDraw/ChaosDDTypes.h"

#if CHAOS_DEBUG_DRAW

namespace Chaos
{
	struct FLatentDrawCommand;
}

namespace ChaosDD::Private
{
	//
	// Primitive rendering API for use by debug draw objects
	//
	class CHAOS_API IChaosDDRenderer
	{
	public:
		IChaosDDRenderer() {}
		virtual ~IChaosDDRenderer() {}

		virtual void DrawCommand(const Chaos::FLatentDrawCommand& Command) const = 0;
	};
}

#endif
