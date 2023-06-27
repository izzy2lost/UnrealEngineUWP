// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "DisplayClusterViewportResourceSettings.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"

#include "Templates/SharedPointer.h"

class FRenderTarget;

/**
 * Runtime state of the viewport resource
 */
enum class EDisplayClusterViewportResourceState : uint8
{
	// empty state, without flags
	None = 0,

	// This resource was initialized in the rendering thread
	Initialized = 1 << 0,

	// This resource is not used for the current frame
	Unused = 1 << 1,

	// Disable reallocation of this resource for the current frame
	DisableReallocate = 1 << 3,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportResourceState);

/**
 * Viewport resource texture base class
 */
class FDisplayClusterViewportResource
	: public TSharedFromThis<FDisplayClusterViewportResource, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewportResource(const FDisplayClusterViewportResourceSettings& InResourceSettings)
		: ResourceSettings(InResourceSettings)
	{ }

	virtual ~FDisplayClusterViewportResource();

public:
	/** Get the current state of the resource. */
	inline EDisplayClusterViewportResourceState& GetResourceState()
	{
		return ResourceState;
	}

	/** Get the current const state of the resource. */
	inline const EDisplayClusterViewportResourceState& GetResourceState() const
	{
		return ResourceState;
	}

	/** Get the current settings of the resource. */
	inline const FDisplayClusterViewportResourceSettings& GetResourceSettings() const
	{
		return ResourceSettings;
	}

public:
	/** Assign external RHI resource. */
	virtual void SetExternalViewportResourceRHI(FTextureRHIRef& InExternalViewportResourceRHI)
	{ }

	/** Get current RHI resource. */
	virtual FRHITexture2D* GetViewportResourceRHI() const
	{
		return nullptr;
	}

	/** Get RenderTarget resource. */
	virtual FRenderTarget* GetViewportResourceRenderTarget()
	{
		return nullptr;
	}

	/** Getting the viewport rect offset in the backbuffer. */
	virtual FIntPoint GetBackbufferFrameOffset() const
	{
		return FIntPoint::ZeroValue;
	}

	/** Settings the viewport rect offset in the backbuffer. */
	virtual void SetBackbufferFrameOffset(const FIntPoint& InBackbufferFrameOffset)
	{ }

	/** RHI initialization for this resource in the rendering thread. */
	virtual void InitializeViewportResource_RenderThread(FRHICommandListBase& RHICmdList)
	{ }

	/** RHI release for this resource in the rendering thread*/
	virtual void ReleaseViewportResource_RenderThread(FRHICommandListBase& RHICmdList)
	{ }

protected:
	/** Helper function: create RTT texture. */
	void ImplInitDynamicRHI_RenderTargetResource2D(FTexture2DRHIRef& OutRenderTargetTextureRHI, FTexture2DRHIRef& OutTextureRHI);

	/** Helper function: create 2D texture. */
	void ImplInitDynamicRHI_TextureResource2D(FTexture2DRHIRef& OutTextureRHI);

private:
	// Settings used by this resource
	const FDisplayClusterViewportResourceSettings ResourceSettings;

	// Resource states
	EDisplayClusterViewportResourceState ResourceState = EDisplayClusterViewportResourceState::None;
};
