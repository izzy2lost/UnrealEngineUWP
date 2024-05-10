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

		// Are we rendering a Server scene?
		virtual bool IsServer() const = 0;

		// Utility functions for use by Debug Draw commands (e.g., FChaosDDLine)
		virtual void RenderLine(const FVector3d& A, const FVector3d& B, const FColor& Color, float LineThickness, float Lifetime) const = 0;
		virtual void RenderBox(const FVector3d& Position, const FQuat4d& Rotation, const FVector3d& Size, const FColor& Color, float LineThickness, float Lifetime) const = 0;

		// Render legacy debug draw command (See FChaosDDScene::RenderLatestFrames)
		virtual void RenderLatentCommand(const Chaos::FLatentDrawCommand& Command) const = 0;
	};
}

#endif
