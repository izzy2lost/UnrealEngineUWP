// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
* Viewport preview-in-scene rendering settings.
*/
struct FDisplayClusterViewport_PreviewSettings
{
	// Preview RTT size multiplier
	float RenderTargetRatioMult = 1.f;

	// Enable/Disable preview rendering. When disabled preview image freeze
	bool bFreezeRender = false;

	// Hack preview gamma.
	// In a scene, PostProcess always renders on top of the preview textures.
	// But in it, PostProcess is also rendered with the flag turned off.
	bool bEnablePostProcess = false;

	// The maximum dimension of any texture for preview
	// Limit preview textures max size
	int32 MaxTextureDimension = 2048;

	// [Experimental] Render preview in multi-GPU
	// Specifies the mGPU index range for rendering the DCRA preview.
	TOptional<FIntPoint> MultiGPURendering;
};
