// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"


FDisplayClusterViewportProxyData::FDisplayClusterViewportProxyData(const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& SrcViewport)
{
	check(IsInGameThread());
	check(SrcViewport.IsValid());

	DstViewportProxy = SrcViewport->ViewportProxy;

	OpenColorIO = SrcViewport->OpenColorIO;

	RenderSettings = SrcViewport->RenderSettings;
	RenderSettingsICVFX.SetParameters(SrcViewport->RenderSettingsICVFX);
	PostRenderSettings.SetParameters(SrcViewport->PostRenderSettings);

	// Additional parameters
	OverscanRuntimeSettings = SrcViewport->OverscanRuntimeSettings;

	RemapMesh = SrcViewport->ViewportRemap.GetRemapMesh();

	ProjectionPolicy = SrcViewport->ProjectionPolicy;
	Contexts         = SrcViewport->Contexts;

	Resources = SrcViewport->Resources;
	ViewStates = SrcViewport->ViewStates;
}

void FDisplayClusterViewportProxyData::UpdateProxy_RenderThread() const
{
	check(IsInRenderingThread());
	check(DstViewportProxy);

	DstViewportProxy->OpenColorIO = OpenColorIO;

	DstViewportProxy->OverscanRuntimeSettings = OverscanRuntimeSettings;

	DstViewportProxy->RemapMesh = RemapMesh;

	DstViewportProxy->RenderSettings = RenderSettings;

	DstViewportProxy->RenderSettingsICVFX.SetParameters(RenderSettingsICVFX);
	DstViewportProxy->PostRenderSettings.SetParameters(PostRenderSettings);

	DstViewportProxy->ProjectionPolicy = ProjectionPolicy;
	
	// The RenderThreadData for DstViewportProxy has been updated in DisplayClusterViewportManagerViewExtension on the rendering thread.
	// Therefore, the RenderThreadData values from the game thread must be overridden by current data from the render thread.
	{
		const TArray<FDisplayClusterViewport_Context> CurrentContexts = DstViewportProxy->Contexts;
		DstViewportProxy->Contexts = Contexts;

		int32 ContextAmmount = FMath::Min(CurrentContexts.Num(), Contexts.Num());
		for (int32 ContextIndex = 0; ContextIndex < ContextAmmount; ContextIndex++)
		{
			DstViewportProxy->Contexts[ContextIndex].RenderThreadData = CurrentContexts[ContextIndex].RenderThreadData;
		}
	}

	// Update viewport proxy resources from container
	DstViewportProxy->Resources = Resources;
	DstViewportProxy->ViewStates = ViewStates;
}
