// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Types of DC viewport resources
 */
enum class EDisplayClusterViewportResourceType : uint8
{
	// Undefined resource type
	Unknown = 0,

	// This RTT is used to render the scene for the viewport eye context to the specified area on this texture.
	// The mGPU rendering with cross-GPU transfer can be used for this texture.
	InternalRenderTargetResource, /** Just for Internal use */


	/**
	 * Internal textures of the DC rendering pipeline.
	 */

	// Unique Viewport contexts shader resource. No regions used at this point
	// The image source for this resource can be: 'InternalRenderTargetResource', 'OverrideTexture', 'OverrideViewport', etc.
	// This is the entry point to the DC rendering pipeline
	InputShaderResource,

	// Special resource with mips texture, generated from 'InputShaderResource' after postprocess
	MipsShaderResource,

	// Additional targetable resource, used by logic (external warpblend, blur, etc)
	// This resource is used as an additional RTT for 'InputShaderResource'.
	// It contains different contents depending on the point in time: 'PostprocessRTT','ViewportOutputRemap','AfterWarpBlend' etc.
	AdditionalTargetableResource,


	/**
	 * Textures for warp and blend
	 */
	 
	 // The viewport texture before warpblend (InputShaderResource)
	// This resource is used as the source image for the projection policy.
	BeforeWarpBlendTargetableResource,

	// The viewport texture after warpblend (AdditionalTargetableResource or InputShaderResource).
	// This resource is used as the output image in the projection policy.
	AfterWarpBlendTargetableResource,


	/**
	 * Output textures
	 */

	// This is the entry point into the output texture rendering.('OutputFrameTargetableResource' or 'OutputPreviewTargetableResource')
	// The rendering time of this image in the DC rendering pipeline is right after warp&blend.
	OutputTargetableResource,


	/**
	 * Output textures for preview rendering.
	 */

	// If DCRA uses a preview, this texture will be used instead of 'Frame' textures
	OutputPreviewTargetableResource,


	/**
	 * Output 'Frame' textures for cluster rendering
	 * The 'Frame' resources are used to compose all the viewports into a single texture.
	 * There is a separate 'frame' texture for each eye context.
	 * And at the end of the frame these resources are copied to the backbuffer.
	 * 
	 * Projection policy render output to this resources into viewport region
	 * (Context frame region saved FDisplayClusterViewport_Context::FrameTargetRect)
	 */

	// This texture contains the results of the DC rendering, and is copied directly to the backbuffer texture at the end of the frame.
	// Each eye is in a separate texture.
	OutputFrameTargetableResource,

	// This resource is used as an additional RTT for 'OutputFrameTargetableResource'.
	// It contains different contents depending on the point in time: 'FramePostprocessRTT','OutputRemap', etc.
	AdditionalFrameTargetableResource,
};

/**
 * Viewport capture mode
 * This mode affects many viewport rendering settings.
 */
enum class EDisplayClusterViewportCaptureMode : uint8
{
	// Use current scene format, no alpha
	Default = 0,

	// use small BGRA 8bit texture with alpha for masking
	Chromakey,

	// use hi-res float texture with alpha for compisiting
	Lightcard,

	// Special hi-res mode for movie pipeline
	MoviePipeline,
};

/**
 * Viewport can be overridden by another one.
 * This mode determines how many resources will be overridden.
 */
enum class EDisplayClusterViewportOverrideMode : uint8
{
	// Do not override this viewport from the other one (Render viewport; create all resources)
	None = 0,

	// Override internalRTT from the other viewport (Don't render this viewport; Don't create RTT resource)
	// Useful for custom PP on the same InRTT. (OCIO per-viewport\node)
	InernalRTT,

	// Override all - clone viewport (Dont render; Don't create resources;)
	All
};
