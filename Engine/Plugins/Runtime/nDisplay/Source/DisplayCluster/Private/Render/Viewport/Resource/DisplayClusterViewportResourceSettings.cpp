// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Resource/DisplayClusterViewportResourceSettings.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"

#include "RHI.h"
#include "RenderResource.h"
#include "UnrealClient.h"

/////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportResource
/////////////////////////////////////////////////////////////////////
FDisplayClusterViewportResourceSettings::FDisplayClusterViewportResourceSettings(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, FViewport* InViewport)
	: ClusterNodeId(InRenderFrameSettings.ClusterNodeId)
{
	if (FRHITexture2D* ViewportTexture = InViewport ? InViewport->GetRenderTargetTexture() : nullptr)
	{
		Format = ViewportTexture->GetFormat();

		if (EnumHasAnyFlags(ViewportTexture->GetFlags(), TexCreate_SRGB))
		{
			EnumAddFlags(ResourceFlags, EDisplayClusterViewportResourceSettingsFlags::ShouldUseSRGB);
		}

		DisplayGamma = InViewport->GetDisplayGamma();
	}
	else
	{
		// Use default settings:
		Format = FDisplayClusterViewportHelpers::GetDefaultPixelFormat();
		DisplayGamma = 2.2f;

		// Always use srgb for preview rendering
		EnumAddFlags(ResourceFlags, EDisplayClusterViewportResourceSettingsFlags::ShouldUseSRGB);

		switch (InRenderFrameSettings.RenderMode)
		{
		case EDisplayClusterRenderFrameMode::PreviewInScene:
			Format = FDisplayClusterViewportHelpers::GetPreviewDefaultPixelFormat();

			// Preview display gamma
			if (InRenderFrameSettings.bPreviewEnablePostProcess == false)
			{
				// Disable postprocess for preview. Use Gamma 1.f
				DisplayGamma = 1.f;
			}
			break;

		default:
			break;
		}
	}
}

FDisplayClusterViewportResourceSettings::FDisplayClusterViewportResourceSettings(const FDisplayClusterViewportResourceSettings& InBaseSettings, const FIntPoint& InSize, const EPixelFormat InFormat, const EDisplayClusterViewportResourceSettingsFlags InResourceFlags, const int32 InNumMips)
	: ClusterNodeId(InBaseSettings.ClusterNodeId)
	, Size(InSize)
	, Format((InFormat == PF_Unknown) ? InBaseSettings.Format : InFormat)
	, DisplayGamma(InBaseSettings.DisplayGamma)
	, NumMips(InNumMips)
	, ResourceFlags(InBaseSettings.ResourceFlags)
{
	EnumAddFlags(ResourceFlags, InResourceFlags);
}
