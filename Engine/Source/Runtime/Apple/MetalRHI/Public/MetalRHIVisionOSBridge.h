// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_VISIONOS
#include "RHIFwd.h"
#import <CompositorServices/CompositorServices.h>

DECLARE_LOG_CATEGORY_EXTERN(LogMetalVisionOS, Warning, All);

namespace MetalRHIVisionOS
{
	METALRHI_API struct BeginRenderingImmersiveParams
    {
        const cp_frame_t SwiftFrame;
    };
    METALRHI_API void BeginRenderingImmersive(const BeginRenderingImmersiveParams& Params);

    METALRHI_API struct PresentImmersiveParams
    {
        const FTextureRHIRef& Texture;
        cp_drawable_t& SwiftDrawable;
    };
    METALRHI_API void PresentImmersive(const PresentImmersiveParams& Params);
}
#endif // PLATFORM_VISIONOS
