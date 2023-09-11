// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "HAL/IConsoleManager.h"


////////////////////////////////////////////////////////////////////////////////
// Experimental feature: to be approved after testing
int32 GDisplayClusterPreviewEnableReuseViewportInCluster = 1;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewEnableReuseViewportInCluster(
	TEXT("DC.Preview.EnableReuseViewportInCluster"),
	GDisplayClusterPreviewEnableReuseViewportInCluster,
	TEXT("Experimental feature (0 == disabled, 1 == enabled)"),
	ECVF_RenderThreadSafe
);

////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterRenderFrameSettings
////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterRenderFrameSettings::IsPreviewRendering() const
{
	return RenderMode == EDisplayClusterRenderFrameMode::PreviewInScene
		|| RenderMode == EDisplayClusterRenderFrameMode::PreviewProxyHitInScene;
}

bool FDisplayClusterRenderFrameSettings::IsPreviewFreezeRender() const
{
	return IsPreviewRendering() && PreviewSettings.bFreezeRender;
}
const FIntPoint* FDisplayClusterRenderFrameSettings::GetPreviewMultiGPURendering() const
{
	if (IsPreviewRendering() && PreviewSettings.MultiGPURendering.IsSet())
	{
		const FIntPoint& GPURange = PreviewSettings.MultiGPURendering.GetValue();
		if (GPURange.X <= GPURange.Y)
		{
			return &GPURange;
		}
	}

	return nullptr;
}

FVector2D FDisplayClusterRenderFrameSettings::GetDesiredFrameMult() const
{
	const float BaseMult = IsPreviewRendering() ? FMath::Clamp(PreviewSettings.RenderTargetRatioMult, 0.f, 1.f) : 1.f;

	switch (RenderMode)
	{
	case EDisplayClusterRenderFrameMode::SideBySide:
		return FVector2D(BaseMult * 0.5f, BaseMult);

	case EDisplayClusterRenderFrameMode::TopBottom:
		return FVector2D(BaseMult, BaseMult * 0.5f);

	default:
		break;
	}

	return FVector2D(BaseMult, BaseMult);
}

bool FDisplayClusterRenderFrameSettings::CanReuseViewportWithinClusterNodes() const
{
	if (GDisplayClusterPreviewEnableReuseViewportInCluster > 0)
	{
		if (IsPreviewRendering())
		{
			return true;
		}
	}

	return false;
}

int32 FDisplayClusterRenderFrameSettings::GetViewportTextureMaxSize() const
{
	if (IsPreviewRendering())
	{
		return PreviewSettings.MaxTextureDimension;
	}

	return -1;
}

bool FDisplayClusterRenderFrameSettings::ShouldUseLinearGamma() const
{
	if (IsPreviewRendering())
	{
		return !PreviewSettings.bEnablePostProcess;
	}

	return false;
}

bool FDisplayClusterRenderFrameSettings::IsPostProcessDisabled() const
{
	if (IsPreviewRendering())
	{
		return !PreviewSettings.bEnablePostProcess;
	}

	return false;
}


int32 FDisplayClusterRenderFrameSettings::GetViewPerViewportAmount() const
{
	switch (RenderMode)
	{
	case EDisplayClusterRenderFrameMode::Stereo:
	case EDisplayClusterRenderFrameMode::SideBySide:
	case EDisplayClusterRenderFrameMode::TopBottom:
		return 2;

	default:
		break;
	}

	return 1;
}
