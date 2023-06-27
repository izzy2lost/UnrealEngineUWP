// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Render/Viewport/Resource/DisplayClusterViewportResource.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"

/**
 * Viewport preview texture resource
 */
class FDisplayClusterViewportPreviewResource
	: public FDisplayClusterViewportResource
{
public:
	FDisplayClusterViewportPreviewResource(const FDisplayClusterViewportResourceSettings& InResourceSettings);
	virtual ~FDisplayClusterViewportPreviewResource() = default;

public:
	//~ Begin FDisplayClusterViewportResource
	virtual FRHITexture2D* GetViewportResourceRHI() const override
	{
		return OutputPreviewTargetableResource.IsValid() ? OutputPreviewTargetableResource->GetTexture2D() : nullptr;
	}

	virtual void SetExternalViewportResourceRHI(FTextureRHIRef& InExternalViewportResourceRHI) override
	{
		OutputPreviewTargetableResource = InExternalViewportResourceRHI;
	}

	virtual void ReleaseViewportResource_RenderThread(FRHICommandListBase& RHICmdList) override
	{
		OutputPreviewTargetableResource.SafeRelease();
	}
	//~~ End FDisplayClusterViewportResource

public:
	// here we implement the preview resources the old way,
	// but when we remove the DCPreviewComponent, this class will be redesigned using UTexture.
	FTextureRHIRef OutputPreviewTargetableResource;
};
