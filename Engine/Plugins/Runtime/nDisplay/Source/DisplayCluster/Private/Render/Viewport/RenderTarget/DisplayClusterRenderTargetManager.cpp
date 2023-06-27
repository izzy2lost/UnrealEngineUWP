// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetManager.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResourcesPool.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Misc/DisplayClusterLog.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////
int32 GDisplayClusterSceneColorFormat = 0;
static FAutoConsoleVariableRef CVarDisplayClusterSceneColorFormat(
	TEXT("nDisplay.render.SceneColorFormat"),
	GDisplayClusterSceneColorFormat,
	TEXT("Defines the memory layout (RGBA) used for the scene color\n"
		"(affects performance, mostly through bandwidth, quality especially with translucency).\n"
		" 0: Use the backbuffer format\n"
		" 1: PF_FloatRGBA 64Bit (default, might be overkill, especially if translucency is mostly using SeparateTranslucency)\n"
		" 2: PF_A32B32G32R32F 128Bit (unreasonable but good for testing)"),
	ECVF_Default
);

////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace UE::DisplayCluster::RenderTargetManager
{
	static EPixelFormat ImplGetCustomFormat(EDisplayClusterViewportCaptureMode CaptureMode)
	{
		EPixelFormat OutPixelFormat = EPixelFormat::PF_Unknown;

		// Pre-defined formats for targets:
		switch (CaptureMode)
		{
		case EDisplayClusterViewportCaptureMode::Chromakey:
		case EDisplayClusterViewportCaptureMode::Lightcard:
			// The Chromakey and LightCard always uses PF_FloatRGBA for OCIO support.
			OutPixelFormat = EPixelFormat::PF_FloatRGBA;
			break;

		case EDisplayClusterViewportCaptureMode::MoviePipeline:
			// Movie pipeline always use PF_FloatRGBA
			OutPixelFormat = EPixelFormat::PF_FloatRGBA;
			break;

		default:
			switch (GDisplayClusterSceneColorFormat)
			{
			case 1:
				OutPixelFormat = EPixelFormat::PF_FloatRGBA;
				break;
			case 2:
				OutPixelFormat = EPixelFormat::PF_A32B32G32R32F;
				break;

			default:
				// by default use the backbuffer format (when function returns PF_Unknown its mean use default scene format)
				break;
			}
			break;
		}

		if (OutPixelFormat != EPixelFormat::PF_Unknown)
		{
			// Fallback in case the scene color selected isn't supported.
			if (!GPixelFormats[OutPixelFormat].Supported)
			{
				OutPixelFormat = PF_FloatRGBA;
			}
		}

		return OutPixelFormat;
	}

	static void ImplViewportTextureResourceLogging(const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& InViewport, const uint32 ContextNum, const FString& ResourceId, const TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>& InTextureResource)
	{
		if (InViewport.IsValid() && InTextureResource != nullptr && EnumHasAnyFlags(InTextureResource->GetResourceState(), EDisplayClusterViewportResourceState::Initialized) == false)
		{
			// Log: New resource created
			UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Created new %s resource (%dx%d) for viewport '%s':%d"), *ResourceId, InTextureResource->GetResourceSettings().GetSizeXY().X, InTextureResource->GetResourceSettings().GetSizeXY().Y, *InViewport->GetId(), ContextNum);
		}
	}
};
using namespace UE::DisplayCluster::RenderTargetManager;

////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDisplayClusterRenderTargetManager
////////////////////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterRenderTargetManager::FDisplayClusterRenderTargetManager(FDisplayClusterViewportManagerProxy* InViewportManagerProxy)
{
	ResourcesPool = MakeUnique<FDisplayClusterRenderTargetResourcesPool>(InViewportManagerProxy);
}

FDisplayClusterRenderTargetManager::~FDisplayClusterRenderTargetManager()
{
	Release();
}

void FDisplayClusterRenderTargetManager::Release()
{
	ResourcesPool->Release();
}

bool FDisplayClusterRenderTargetManager::AllocateRenderFrameResources(FViewport* InViewport, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& InViewports, FDisplayClusterRenderFrame& InOutRenderFrame)
{
	bool bResult = true;

	if(ResourcesPool->BeginReallocateResources(InRenderFrameSettings, InViewport))
	{
		// ReAllocate Render targets for all viewports
		for (FDisplayClusterRenderFrameTarget& FrameRenderTargetIt : InOutRenderFrame.RenderTargets)
		{
			if (FrameRenderTargetIt.bShouldUseRenderTarget)
			{
				// reallocate
				TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> NewResource = ResourcesPool->AllocateResource(FrameRenderTargetIt.RenderTargetSize, ImplGetCustomFormat(FrameRenderTargetIt.CaptureMode), EDisplayClusterViewportResourceSettingsFlags::RenderTarget);
				if (NewResource.IsValid())
				{
					// Set RenderFrame resource
					FrameRenderTargetIt.RenderTargetPtr = NewResource->GetViewportResourceRenderTarget();
					check(FrameRenderTargetIt.RenderTargetPtr);

					// Assign for all views in render target families
					for (FDisplayClusterRenderFrameTargetViewFamily& ViewFamily : FrameRenderTargetIt.ViewFamilies)
					{
						for (FDisplayClusterRenderFrameTargetView& ViewIt : ViewFamily.Views)
						{
							if (FDisplayClusterViewport* ViewportPtr = static_cast<FDisplayClusterViewport*>(ViewIt.Viewport.Get()))
							{
								// Array already resized in function FDisplayClusterViewport::UpdateFrameContexts() with RenderTargets.AddZeroed(ViewportContextAmount);
								check(ViewportPtr->Resources[EDisplayClusterViewportResource::RenderTargets].IsValidIndex(ViewIt.ContextNum));
								check(!ViewportPtr->Contexts[ViewIt.ContextNum].bDisableRender);

								ViewportPtr->Resources[EDisplayClusterViewportResource::RenderTargets][ViewIt.ContextNum] = NewResource;

								if (EnumHasAnyFlags(NewResource->GetResourceState(), EDisplayClusterViewportResourceState::Initialized) == false)
								{
									// Log: New resource created
									UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Created new ViewportRenderTarget resource %08X (%dx%d) for viewport '%s'"), NewResource.Get(), NewResource->GetResourceSettings().GetSizeX(), NewResource->GetResourceSettings().GetSizeY(), *ViewportPtr->GetId());
								}
							}
						}
					}
				}
			}
		}

		// Allocate viewport internal resources
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : InViewports)
		{
			if (ViewportIt.IsValid() && ViewportIt->RenderSettings.bFreezeRendering == false)
			{
				EPixelFormat CustomFormat = ImplGetCustomFormat(ViewportIt->RenderSettings.CaptureMode);

				// Allocate all context resources:
				for (const FDisplayClusterViewport_Context& ContextIt : ViewportIt->GetContexts())
				{
					const FIntPoint& ContextSize = ContextIt.ContextSize;
					const uint32& ContextNum = ContextIt.ContextNum;

					// Allocate per-viewport preview output textures:
					if (ViewportIt->Resources[EDisplayClusterViewportResource::OutputPreviewTargetableResources].Num() > (int32)ContextNum)
					{
						ViewportIt->Resources[EDisplayClusterViewportResource::OutputPreviewTargetableResources][ContextNum] = ResourcesPool->AllocateResource(ContextSize, CustomFormat, EDisplayClusterViewportResourceSettingsFlags::PreviewTargetableTexture);
						ImplViewportTextureResourceLogging(ViewportIt, ContextNum, TEXT("InputShader"), ViewportIt->Resources[EDisplayClusterViewportResource::OutputPreviewTargetableResources][ContextNum]);
					}

					if (ViewportIt->Resources[EDisplayClusterViewportResource::InputShaderResources].Num() > (int32)ContextNum)
					{
						ViewportIt->Resources[EDisplayClusterViewportResource::InputShaderResources][ContextNum] = ResourcesPool->AllocateResource(ContextSize, CustomFormat, EDisplayClusterViewportResourceSettingsFlags::ResolveTargetableTexture);
						ImplViewportTextureResourceLogging(ViewportIt, ContextNum, TEXT("InputShader"), ViewportIt->Resources[EDisplayClusterViewportResource::InputShaderResources][ContextNum]);
					}

					// Allocate custom resources:
					if (ViewportIt->Resources[EDisplayClusterViewportResource::AdditionalTargetableResources].Num() > (int32)ContextNum)
					{
						ViewportIt->Resources[EDisplayClusterViewportResource::AdditionalTargetableResources][ContextNum] = ResourcesPool->AllocateResource(ContextSize, CustomFormat, EDisplayClusterViewportResourceSettingsFlags::RenderTargetableTexture);
						ImplViewportTextureResourceLogging(ViewportIt, ContextNum, TEXT("AdditionalRTT"), ViewportIt->Resources[EDisplayClusterViewportResource::AdditionalTargetableResources][ContextNum]);
					}

					if (ViewportIt->Resources[EDisplayClusterViewportResource::MipsShaderResources].Num() > (int32)ContextNum)
					{
						ViewportIt->Resources[EDisplayClusterViewportResource::MipsShaderResources][ContextNum] = ResourcesPool->AllocateResource(ContextSize, CustomFormat, EDisplayClusterViewportResourceSettingsFlags::ResolveTargetableTexture, ContextIt.NumMips);
						ImplViewportTextureResourceLogging(ViewportIt, ContextNum, TEXT("Mips"), ViewportIt->Resources[EDisplayClusterViewportResource::MipsShaderResources][ContextNum]);
					}
				}
			}
		}

		// Allocate frame targets for all visible  on backbuffer viewports
		FIntPoint ViewportSize = InViewport ? InViewport->GetSizeXY() : FIntPoint(0, 0);

		if (!AllocateFrameTargets(InRenderFrameSettings, ViewportSize, InOutRenderFrame))
		{
			UE_LOG(LogDisplayClusterViewport, Error, TEXT("DisplayClusterRenderTargetManager: Can't allocate frame targets."));
			bResult = false;
		}

		ResourcesPool->EndReallocateResources();
	}

	return bResult;
}

bool FDisplayClusterRenderTargetManager::AllocateFrameTargets(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const FIntPoint& InViewportSize, FDisplayClusterRenderFrame& InOutRenderFrame)
{
	uint32 FrameTargetsAmount = 0;

	// Support side_by_side and top_bottom eye offset, aligned to Viewport size:
	FIntPoint TargetLocation(ForceInitToZero);
	FIntPoint TargetOffset(ForceInitToZero);

	//Re-allocate frame targets
	switch (InRenderFrameSettings.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		// Preview model: render to external RTT resource
		return true;

	case EDisplayClusterRenderFrameMode::Mono:
		FrameTargetsAmount = 1;
		break;

	case EDisplayClusterRenderFrameMode::Stereo:
		FrameTargetsAmount = 2;
		break;

	case EDisplayClusterRenderFrameMode::SideBySide:
		FrameTargetsAmount = 2;
		TargetOffset.X = InViewportSize.X / 2;
		break;

	case EDisplayClusterRenderFrameMode::TopBottom:
		FrameTargetsAmount = 2;
		TargetOffset.Y = InViewportSize.Y / 2;
		break;

	default:
		// skip not implemented cases
		return false;
	}

	// Reallocate frame target resources
	TArray<TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>> NewFrameTargetResources;
	TArray<TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>> NewAdditionalFrameTargetableResources;

	for (uint32 FrameTargetsIt = 0; FrameTargetsIt < FrameTargetsAmount; FrameTargetsIt++)
	{
		TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> NewResource = ResourcesPool->AllocateResource(InOutRenderFrame.FrameRect.Size(), PF_Unknown, EDisplayClusterViewportResourceSettingsFlags::RenderTargetableTexture);
		if (NewResource.IsValid())
		{
			if (EnumHasAnyFlags(NewResource->GetResourceState(), EDisplayClusterViewportResourceState::Initialized) == false)
			{
				// Log: New resource created
				UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Created new RenderFrame resource (%dx%d)"), NewResource->GetResourceSettings().GetSizeX(), NewResource->GetResourceSettings().GetSizeY());
			}

			// calc and assign backbuffer offset (side_by_side, top_bottom)
			NewResource->SetBackbufferFrameOffset(InOutRenderFrame.FrameRect.Min + TargetLocation);
			TargetLocation += TargetOffset;
			NewFrameTargetResources.Add(NewResource);
		}

		if (InRenderFrameSettings.bShouldUseAdditionalFrameTargetableResource)
		{
			TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> NewAdditionalResource = ResourcesPool->AllocateResource(InOutRenderFrame.FrameRect.Size(), PF_Unknown, EDisplayClusterViewportResourceSettingsFlags::RenderTargetableTexture);
			if (NewAdditionalResource.IsValid())
			{
				if (EnumHasAnyFlags(NewAdditionalResource->GetResourceState(), EDisplayClusterViewportResourceState::Initialized) == false)
				{
					// Log: New resource created
					UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Created new RenderFrame2 resource (%dx%d)"), NewAdditionalResource->GetResourceSettings().GetSizeX(), NewAdditionalResource->GetResourceSettings().GetSizeY());
				}

				NewAdditionalFrameTargetableResources.Add(NewAdditionalResource);
			}
		}
	}

	// Assign frame resources for all visible viewports
	for (FDisplayClusterRenderFrameTarget& RenderTargetIt : InOutRenderFrame.RenderTargets)
	{
		for (FDisplayClusterRenderFrameTargetViewFamily& ViewFamilieIt : RenderTargetIt.ViewFamilies)
		{
			for (FDisplayClusterRenderFrameTargetView& ViewIt : ViewFamilieIt.Views)
			{
				FDisplayClusterViewport* ViewportPtr = static_cast<FDisplayClusterViewport*>(ViewIt.Viewport.Get());
				if (ViewportPtr && ViewportPtr->RenderSettings.bVisible)
				{
					ViewportPtr->Resources[EDisplayClusterViewportResource::OutputFrameTargetableResources] = NewFrameTargetResources;
					ViewportPtr->Resources[EDisplayClusterViewportResource::AdditionalFrameTargetableResources] = NewAdditionalFrameTargetableResources;

					// Adjust viewports frame rects. This offset saved in 'BackbufferFrameOffset'
					if (ViewportPtr->Contexts.IsValidIndex(ViewIt.ContextNum))
					{
						FDisplayClusterViewport_Context& Context = ViewportPtr->Contexts[ViewIt.ContextNum];
						Context.FrameTargetRect.Min -= InOutRenderFrame.FrameRect.Min;
						Context.FrameTargetRect.Max -= InOutRenderFrame.FrameRect.Min;
					}
				}
			}
		}
	}

	return true;
}
