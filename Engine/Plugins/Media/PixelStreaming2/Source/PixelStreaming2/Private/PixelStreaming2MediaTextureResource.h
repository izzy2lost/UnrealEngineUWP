// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreaming2MediaTexture.h"
#include "TextureResource.h"

/**
 * The actual texture resource for a FPixelStreamingMediaTexture. Contains the RHI Texture and
 * Sampler information.
 */
class FPixelStreaming2MediaTextureResource : public FTextureResource
{
public:
	FPixelStreaming2MediaTextureResource(TWeakObjectPtr<UPixelStreaming2MediaTexture> Owner);
	virtual ~FPixelStreaming2MediaTextureResource() override { TextureRHI.SafeRelease(); }

	virtual void   InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void   ReleaseRHI() override;
	virtual uint32 GetSizeX() const override;
	virtual uint32 GetSizeY() const override;

	SIZE_T GetResourceSize();

private:
	TWeakObjectPtr<UPixelStreaming2MediaTexture> MediaTexture;
};
