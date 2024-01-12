// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManagerProxy.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Containers/IDisplayClusterRender_MeshComponent.h"
#include "Render/Containers/IDisplayClusterRender_MeshComponentProxy.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "IDisplayClusterShaders.h"
#include "TextureResource.h"

#include "RHIStaticStates.h"

#include "RenderResource.h"
#include "RenderingThread.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"

#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

#include "ScreenRendering.h"
#include "PostProcess/PostProcessAA.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "PostProcess/DrawRectangle.h"

#include "ScreenPass.h"
#include "SceneTextures.h"
#include "PostProcess/PostProcessMaterialInputs.h"

static TAutoConsoleVariable<int32> CVarDisplayClusterRenderOverscanResolve(
	TEXT("nDisplay.render.overscan.resolve"),
	1,
	TEXT("Allow resolve overscan internal rect to output backbuffer.\n")
	TEXT(" 0 - to disable.\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterShadersICVFXFXAALightCard = 2;
static FAutoConsoleVariableRef CVarDisplayClusterShadersICVFXFXAALightCard(
	TEXT("nDisplay.render.icvfx.fxaa.lightcard"),
	GDisplayClusterShadersICVFXFXAALightCard,
	TEXT("FXAA quality for lightcard (0 - disable).\n")
	TEXT("1..6 - FXAA quality from Lowest Quality(Fastest) to Highest Quality(Slowest).\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterShadersICVFXFXAAChromakey = 2;
static FAutoConsoleVariableRef CVarDisplayClusterShadersICVFXFXAAChromakey(
	TEXT("nDisplay.render.icvfx.fxaa.chromakey"),
	GDisplayClusterShadersICVFXFXAAChromakey,
	TEXT("FXAA quality for chromakey (0 - disable).\n")
	TEXT("1..6 - FXAA quality from Lowest Quality(Fastest) to Highest Quality(Slowest).\n"),
	ECVF_RenderThreadSafe
);

// Enable/disable warp&blend
int32 GDisplayClusterRenderWarpBlendEnabled = 1;
static FAutoConsoleVariableRef CVarDisplayClusterRenderWarpBlendEnabled(
	TEXT("nDisplay.render.WarpBlendEnabled"),
	GDisplayClusterRenderWarpBlendEnabled,
	TEXT("Warp & Blend status\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
namespace UE::DisplayCluster::ViewportProxy
{
	static IDisplayClusterShaders& GetShadersAPI()
	{
		static IDisplayClusterShaders& ShadersAPISingleton = IDisplayClusterShaders::Get();

		return ShadersAPISingleton;
	}

	// The viewport override has the maximum depth. This protects against a link cycle
	static const int32 DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax = 4;

	static bool GetFXAAQuality(const EDisplayClusterViewportCaptureMode InCaptureMode, EFXAAQuality& OutFXAAQuality)
	{
		// Get FXAA quality for current viewport
		EFXAAQuality FXAAQuality = EFXAAQuality::MAX;

		switch (InCaptureMode)
		{
		case EDisplayClusterViewportCaptureMode::Chromakey:
			if (GDisplayClusterShadersICVFXFXAAChromakey > 0)
			{
				OutFXAAQuality = (EFXAAQuality)FMath::Clamp(GDisplayClusterShadersICVFXFXAAChromakey - 1, 0, (int32)EFXAAQuality::MAX - 1);
				return true;
			}
			break;

		case EDisplayClusterViewportCaptureMode::Lightcard:
			if (GDisplayClusterShadersICVFXFXAALightCard > 0)
			{
				OutFXAAQuality = (EFXAAQuality)FMath::Clamp(GDisplayClusterShadersICVFXFXAALightCard - 1, 0, (int32)EFXAAQuality::MAX - 1);
				return true;
			}
			break;

		default:
			break;
		}

		// No FXAA
		return false;
	}

	template<class TScreenPixelShader>
	void ResampleCopyTextureImpl_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect& SrcRect, const FIntRect& DstRect, const EDisplayClusterTextureCopyMode InCopyMode = EDisplayClusterTextureCopyMode::RGBA)
	{
		// Texture format mismatch, use a shader to do the copy.
		// #todo-renderpasses there's no explicit resolve here? Do we need one?
		FRHIRenderPassInfo RPInfo(DstTexture, ERenderTargetActions::Load_Store);
		RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

		RHICmdList.BeginRenderPass(RPInfo, TEXT("DisplayCluster_ResampleCopyTexture"));
		{
			FIntVector SrcSizeXYZ = SrcTexture->GetSizeXYZ();
			FIntVector DstSizeXYZ = DstTexture->GetSizeXYZ();

			FIntPoint SrcSize(SrcSizeXYZ.X, SrcSizeXYZ.Y);
			FIntPoint DstSize(DstSizeXYZ.X, DstSizeXYZ.Y);

			RHICmdList.SetViewport(0.f, 0.f, 0.0f, DstSize.X, DstSize.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			switch (InCopyMode)
			{
			case EDisplayClusterTextureCopyMode::Alpha:
				// Copy alpha channel from source to dest
				GraphicsPSOInit.BlendState = TStaticBlendState <CW_ALPHA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
				break;

			case EDisplayClusterTextureCopyMode::RGB:
				// Copy only RGB channels from source to dest
				GraphicsPSOInit.BlendState = TStaticBlendState <CW_RGB, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
				break;

			case EDisplayClusterTextureCopyMode::RGBA:
			default:
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				break;
			}

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
			TShaderMapRef<TScreenPixelShader> PixelShader(ShaderMap);
			if (!VertexShader.IsValid() || !PixelShader.IsValid())
			{
				// Always check if shaders are available on the current platform and hardware
				return;
			}

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			const bool bSameSize = (SrcRect.Size() == DstRect.Size());
			FRHISamplerState* PixelSampler = bSameSize ? TStaticSamplerState<SF_Point>::GetRHI() : TStaticSamplerState<SF_Bilinear>::GetRHI();

			SetShaderParametersLegacyPS(RHICmdList, PixelShader, PixelSampler, SrcTexture);

			// Set up vertex uniform parameters for scaling and biasing the rectangle.
			// Note: Use DrawRectangle in the vertex shader to calculate the correct vertex position and uv.

			UE::Renderer::PostProcess::DrawRectangle(
				RHICmdList, VertexShader,
				DstRect.Min.X, DstRect.Min.Y,
				DstRect.Size().X, DstRect.Size().Y,
				SrcRect.Min.X, SrcRect.Min.Y,
				SrcRect.Size().X, SrcRect.Size().Y,
				DstSize, SrcSize
			);
		}
		RHICmdList.EndRenderPass();
		RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}

	static void ImplResolveResource(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InputResource, const FIntRect& InputRect, FRHITexture2D* OutputResource, const FIntRect& OutputRect, const bool bOutputIsMipsResource, const bool bOutputIsPreviewResource)
	{
		check(InputResource);
		check(OutputResource);

		if (bOutputIsPreviewResource)
		{
			// The preview texture should use only RGB colors and ignore the alpha channel. The alpha channel may or may not be inverted in third-party libraries.
			ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, InputResource, OutputResource, InputRect, OutputRect, EDisplayClusterTextureCopyMode::RGB);
		}
		else if (InputRect.Size() == OutputRect.Size() && InputResource->GetFormat() == OutputResource->GetFormat())
		{
			FRHICopyTextureInfo CopyInfo;
			CopyInfo.Size = FIntVector(InputRect.Width(), InputRect.Height(), 0);
			CopyInfo.SourcePosition.X = InputRect.Min.X;
			CopyInfo.SourcePosition.Y = InputRect.Min.Y;
			CopyInfo.DestPosition.X = OutputRect.Min.X;
			CopyInfo.DestPosition.Y = OutputRect.Min.Y;

			TransitionAndCopyTexture(RHICmdList, InputResource, OutputResource, CopyInfo);
		}
		else
		{
			ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, InputResource, OutputResource, InputRect, OutputRect);
		}
	}


	struct FViewportResourceResolverData
	{
		FViewportResourceResolverData(const FDisplayClusterViewport_Context& InViewportContext, FRHITexture2D* InInputResource, const FIntRect& InInputRect, FRHITexture2D* InOutputResource, const FIntRect& InOutputRect, const bool InbOutputIsMipsResource, const bool InbOutputIsPreviewResource)
			: InputResource(InInputResource)
			, OutputResource(InOutputResource)
			, InputRect(InInputRect)
			, OutputRect(InOutputRect)
			, bOutputIsMipsResource(InbOutputIsMipsResource)
			, bOutputIsPreviewResource(InbOutputIsPreviewResource)
			, ViewportContext(InViewportContext)
		{ }

		void Resolve_RenderThread(FRHICommandListImmediate& RHICmdList)
		{
			ImplResolveResource(RHICmdList, InputResource, InputRect, OutputResource, OutputRect, bOutputIsMipsResource, bOutputIsPreviewResource);
		}

		void AddFinalPass_RenderThread(FRDGBuilder& GraphBuilder, IDisplayClusterDisplayDeviceProxy& DisplayDevice)
		{
			DisplayDevice.AddFinalPass_RenderThread(GraphBuilder, ViewportContext, InputResource, InputRect, OutputResource, OutputRect);
		}

	private:
		FRHITexture2D* InputResource;
		FRHITexture2D* OutputResource;

		const FIntRect InputRect;
		const FIntRect OutputRect;

		const bool bOutputIsMipsResource;
		const bool bOutputIsPreviewResource;

		const FDisplayClusterViewport_Context ViewportContext;
	};
};

///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportProxy::FDisplayClusterViewportProxy(const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration, const FString& InViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy)
	: ConfigurationProxy(InConfiguration->Proxy)
	, ViewportId(InViewportId)
	, ClusterNodeId(InConfiguration->GetClusterNodeId())
	, ProjectionPolicy(InProjectionPolicy)
{
	check(ProjectionPolicy.IsValid());
}

FDisplayClusterViewportProxy::~FDisplayClusterViewportProxy()
{
}

void FDisplayClusterViewportProxy::UpdateViewportProxyData_RenderThread(const FDisplayClusterViewportProxyData& InViewportProxyData)
{
	OpenColorIO = InViewportProxyData.OpenColorIO;

	DisplayDeviceProxy = InViewportProxyData.DisplayDeviceProxy;

	OverscanRuntimeSettings = InViewportProxyData.OverscanRuntimeSettings;

	RemapMesh = InViewportProxyData.RemapMesh;

	RenderSettings = InViewportProxyData.RenderSettings;

	RenderSettingsICVFX.SetParameters(InViewportProxyData.RenderSettingsICVFX);
	PostRenderSettings.SetParameters(InViewportProxyData.PostRenderSettings);

	ProjectionPolicy = InViewportProxyData.ProjectionPolicy;

	// The RenderThreadData for DstViewportProxy has been updated in DisplayClusterViewportManagerViewExtension on the rendering thread.
	// Therefore, the RenderThreadData values from the game thread must be overridden by current data from the render thread.
	{
		const TArray<FDisplayClusterViewport_Context> CurrentContexts = Contexts;
		Contexts = InViewportProxyData.Contexts;

		int32 ContextAmmount = FMath::Min(CurrentContexts.Num(), Contexts.Num());
		for (int32 ContextIndex = 0; ContextIndex < ContextAmmount; ContextIndex++)
		{
			Contexts[ContextIndex].RenderThreadData = CurrentContexts[ContextIndex].RenderThreadData;
		}
	}

	// Update viewport proxy resources from container
	Resources = InViewportProxyData.Resources;
	ViewStates = InViewportProxyData.ViewStates;
}

EDisplayClusterViewportResourceType FDisplayClusterViewportProxy::GetResourceType_RenderThread(const EDisplayClusterViewportResourceType& InResourceType) const
{
	check(IsInRenderingThread());

	switch (InResourceType)
	{
	case EDisplayClusterViewportResourceType::BeforeWarpBlendTargetableResource:
		return EDisplayClusterViewportResourceType::InputShaderResource;

	case EDisplayClusterViewportResourceType::AfterWarpBlendTargetableResource:
		return ShouldApplyWarpBlend_RenderThread() ? EDisplayClusterViewportResourceType::AdditionalTargetableResource : EDisplayClusterViewportResourceType::InputShaderResource;

	case EDisplayClusterViewportResourceType::OutputTargetableResource:

		/**
		* Output textures for preview rendering.
		*/
		if (ConfigurationProxy->IsPreviewRendering_RenderThread())
		{
			// [warp] -> OutputPreviewTargetableResource
			return EDisplayClusterViewportResourceType::OutputPreviewTargetableResource;
		}

		/**
		* Output 'Frame' textures for cluster rendering
		*/
		if (RemapMesh.IsValid())
		{
			const IDisplayClusterRender_MeshComponentProxy* MeshProxy = RemapMesh->GetMeshComponentProxy_RenderThread();
			if (MeshProxy != nullptr && MeshProxy->IsEnabled_RenderThread())
			{
				// In this case render to additional frame targetable
				// to minimize the rendering steps:
				// [warp] -> AdditionalFrameTargetableResource -> [OutputRemap] -> OutputFrameTargetableResource
				return EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource;
			}
		}

		// [warp] -> OutputFrameTargetableResource
		return EDisplayClusterViewportResourceType::OutputFrameTargetableResource;


	default:
		// return the same value
		break;
	}

	return InResourceType;
}

bool FDisplayClusterViewportProxy::ShouldApplyWarpBlend_RenderThread() const
{
	if (GDisplayClusterRenderWarpBlendEnabled == 0)
	{
		return false;
	}

	if (GetPostRenderSettings_RenderThread().Replace.IsEnabled())
	{
		// When used override texture, disable warp blend
		return false;
	}

	if (!ConfigurationProxy->GetRenderFrameSettings().bAllowWarpBlend)
	{
		return false;
	}

	// Ask current projection policy if it's warp&blend compatible
	return ProjectionPolicy.IsValid() && ProjectionPolicy->IsWarpBlendSupported();
}

bool FDisplayClusterViewportProxy::ShouldOverrideViewportResource(const EDisplayClusterViewportResourceType InExtResourceType) const
{
	// Override resources from other viewport
	if (RenderSettings.IsViewportOverridden())
	{
		switch (RenderSettings.GetViewportOverrideMode())
		{
		case EDisplayClusterViewportOverrideMode::All:
		{
			switch (GetResourceType_RenderThread(InExtResourceType))
			{
			case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
			case EDisplayClusterViewportResourceType::InputShaderResource:
			case EDisplayClusterViewportResourceType::MipsShaderResource:
			case EDisplayClusterViewportResourceType::AdditionalTargetableResource:
				return true;

			default:
				break;
			}
		}
		break;

		case EDisplayClusterViewportOverrideMode::InernalRTT:
		{
			switch (GetResourceType_RenderThread(InExtResourceType))
			{
			case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
				return true;

			default:
				break;
			}
		}
		break;

		default:
			break;
		}
	}

	return false;
}

//  Return viewport scene proxy resources by type
bool FDisplayClusterViewportProxy::GetResources_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture2D*>& OutResources) const
{
	return ImplGetResources_RenderThread(InExtResourceType, OutResources, false);
}

const FDisplayClusterViewportProxy& FDisplayClusterViewportProxy::GetRenderingViewportProxy() const
{
	switch (RenderSettings.GetViewportOverrideMode())
	{
	case EDisplayClusterViewportOverrideMode::All:
	case EDisplayClusterViewportOverrideMode::InernalRTT:
		if (FDisplayClusterViewportManagerProxy* ViewportManagerProxy = ConfigurationProxy->GetViewportManagerProxyImpl())
		{
			if (FDisplayClusterViewportProxy const* OverrideViewportProxy = ViewportManagerProxy->ImplFindViewportProxy_RenderThread(RenderSettings.GetViewportOverrideId()))
			{
				return *OverrideViewportProxy;
			}
		}

		break;
	}

	return *this;
}

bool FDisplayClusterViewportProxy::IsInputRenderTargetResourceExists() const
{
	if (PostRenderSettings.Replace.IsEnabled())
	{
		// Use external texture
		return true;
	}

	if (EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		// Use external UVLightCard Resource
		return true;
	}

	return !Resources[EDisplayClusterViewportResource::RenderTargets].IsEmpty();
}

bool FDisplayClusterViewportProxy::ImplGetResources_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture2D*>& OutResources, const int32 InRecursionDepth) const
{
	using namespace UE::DisplayCluster::ViewportProxy;
	check(IsInRenderingThread());

	const EDisplayClusterViewportResourceType InResourceType = GetResourceType_RenderThread(InExtResourceType);

	// Override resources from other viewport
	if (ShouldOverrideViewportResource(InResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			return GetRenderingViewportProxy().ImplGetResources_RenderThread(InExtResourceType, OutResources, InRecursionDepth + 1);
		}

		return false;
	}

	OutResources.Empty();

	switch (InResourceType)
	{
	case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
	{
		bool bResult = false;

		if (Contexts.Num() > 0)
		{

			// 1. Replace RTT from configuration
			if (!bResult && PostRenderSettings.Replace.IsEnabled())
			{
				bResult = true;

				// Support texture replace:
				if (FRHITexture2D* ReplaceTextureRHI = PostRenderSettings.Replace.TextureRHI->GetTexture2D())
				{
					for (int32 ContextIndex = 0; ContextIndex < Contexts.Num(); ContextIndex++)
					{
						OutResources.Add(ReplaceTextureRHI);
					}
				}
			}

			// 2. Replace RTT from UVLightCard:
			if (!bResult && EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
			{
				bResult = true;
				
				// Get resources from external UV LightCard manager
				if (FDisplayClusterViewportManagerProxy* ViewportManagerProxy = ConfigurationProxy->GetViewportManagerProxyImpl())
				{
					TSharedPtr<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> LightCardManager = ViewportManagerProxy->GetLightCardManagerProxy_RenderThread();
					if (LightCardManager.IsValid())
					{
						if (FRHITexture* UVLightCardRHIResource = LightCardManager->GetUVLightCardRHIResource_RenderThread())
						{
							for (int32 ContextIndex = 0; ContextIndex < Contexts.Num(); ContextIndex++)
							{
								OutResources.Add(UVLightCardRHIResource);
							}
						}
					}
				}
			}

			// 3. Finally Use InternalRTT
			if (!bResult)
			{
				bResult = Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::RenderTargets, OutResources);
			}
		}

		if (!bResult || Contexts.Num() != OutResources.Num())
		{
			OutResources.Empty();
		}

		return OutResources.Num() > 0;
	}

	case EDisplayClusterViewportResourceType::InputShaderResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::InputShaderResources, OutResources);

	case EDisplayClusterViewportResourceType::AdditionalTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::AdditionalTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::MipsShaderResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::MipsShaderResources, OutResources);

	case EDisplayClusterViewportResourceType::OutputFrameTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::OutputFrameTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::AdditionalFrameTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::OutputPreviewTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::OutputPreviewTargetableResources, OutResources);

	default:
		break;
	}

	return false;
}

EDisplayClusterViewportOpenColorIOMode FDisplayClusterViewportProxy::GetOpenColorIOMode() const
{
	if (OpenColorIO.IsValid() && OpenColorIO->IsValid_RenderThread())
	{
		if (EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard | EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::Chromakey)
			|| ConfigurationProxy->GetRenderFrameSettings().IsPostProcessDisabled())
		{
			// Rendering without post-processing, OCIO is applied last, to the RTT texture of the viewport
			return EDisplayClusterViewportOpenColorIOMode::Resolved;
		}
		else if (RenderSettings.bForceLateOCIOPass)
		{
			// When capturing a viewport, it's possible that it's going to be shared within a cluster via the media pipeline.
			// In this case we should postpone the OCIO step so every node can apply its own OCIO settings.
			return EDisplayClusterViewportOpenColorIOMode::Resolved;
		}

		// By default, viewports render with a postprocess, OCIO must be done in between.
		return EDisplayClusterViewportOpenColorIOMode::PostProcess;
	}

	return EDisplayClusterViewportOpenColorIOMode::None;
}

void FDisplayClusterViewportProxy::PostResolveViewport_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	// resolve warped viewport resource to the output texture
	ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::AfterWarpBlendTargetableResource, EDisplayClusterViewportResourceType::OutputTargetableResource);

	// Implement ViewportRemap feature
	ImplViewportRemap_RenderThread(RHICmdList);
}

void FDisplayClusterViewportProxy::ImplViewportRemap_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	using namespace UE::DisplayCluster::ViewportProxy;

	// Preview in editor not support this feature
	if (ConfigurationProxy->IsPreviewRendering_RenderThread())
	{
		return;
	}

	if (RemapMesh.IsValid())
	{
		const IDisplayClusterRender_MeshComponentProxy* MeshProxy = RemapMesh->GetMeshComponentProxy_RenderThread();
		if (MeshProxy!=nullptr && MeshProxy->IsEnabled_RenderThread())
		{
			if (Resources[EDisplayClusterViewportResource::AdditionalFrameTargetableResources].Num() != Resources[EDisplayClusterViewportResource::OutputFrameTargetableResources].Num())
			{
				// error
				return;
			}

			for (int32 ContextIt = 0; ContextIt < Resources[EDisplayClusterViewportResource::AdditionalFrameTargetableResources].Num(); ContextIt++)
			{
				const TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>& Src = Resources[EDisplayClusterViewportResource::AdditionalFrameTargetableResources][ContextIt];
				const TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>& Dst = Resources[EDisplayClusterViewportResource::OutputFrameTargetableResources][ContextIt];

				FRHITexture2D* Input = Src.IsValid() ? Src->GetViewportResourceRHI_RenderThread() : nullptr;
				FRHITexture2D* Output = Dst.IsValid() ? Dst->GetViewportResourceRHI_RenderThread() : nullptr;

				if (Input && Output)
				{
					GetShadersAPI().RenderPostprocess_OutputRemap(RHICmdList, Input, Output, *MeshProxy);
				}
			}
		}
	}
}

bool FDisplayClusterViewportProxy::GetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutResourceRects) const
{
	return ImplGetResourcesWithRects_RenderThread(InExtResourceType, OutResources, OutResourceRects, false);
}

bool FDisplayClusterViewportProxy::ImplGetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutResourceRects, const int32 InRecursionDepth) const
{
	using namespace UE::DisplayCluster::ViewportProxy;
	check(IsInRenderingThread());

	// Override resources from other viewport
	if(ShouldOverrideViewportResource(InExtResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			return GetRenderingViewportProxy().ImplGetResourcesWithRects_RenderThread(InExtResourceType, OutResources, OutResourceRects, InRecursionDepth + 1);
		}

		return false;
	}

	const EDisplayClusterViewportResourceType InResourceType = GetResourceType_RenderThread(InExtResourceType);

	if (!GetResources_RenderThread(InResourceType, OutResources))
	{
		return false;
	}

	// Collect all resource rects:
	OutResourceRects.AddDefaulted(OutResources.Num());

	switch (InResourceType)
	{
	case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
		for (int32 ContextIt = 0; ContextIt < OutResourceRects.Num(); ContextIt++)
		{
			if (PostRenderSettings.Replace.IsEnabled())
			{
				// Get image from Override
				OutResourceRects[ContextIt] = PostRenderSettings.Replace.Rect;
			}
			else
			{
				OutResourceRects[ContextIt] = Contexts[ContextIt].RenderTargetRect;
			}
		}
		break;
	case EDisplayClusterViewportResourceType::OutputFrameTargetableResource:
	case EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource:
		for (int32 ContextIt = 0; ContextIt < OutResourceRects.Num(); ContextIt++)
		{
			OutResourceRects[ContextIt] = Contexts[ContextIt].FrameTargetRect;
		}
		break;
	default:
		for (int32 ContextIt = 0; ContextIt < OutResourceRects.Num(); ContextIt++)
		{
			OutResourceRects[ContextIt] = FIntRect(FIntPoint(0, 0), OutResources[ContextIt]->GetSizeXY());
		}
	}

	return true;
}

bool FDisplayClusterViewportProxy::ApplyOCIO_RenderThread(FRHICommandListImmediate& RHICmdList, const FDisplayClusterViewportProxy& InSrcViewportProxy, const EDisplayClusterViewportResourceType InSrcResourceType) const
{
	if (GetOpenColorIOMode() != EDisplayClusterViewportOpenColorIOMode::Resolved)
	{
		return false;
	}

	const EDisplayClusterViewportResourceType DestResourceType = (InSrcResourceType == EDisplayClusterViewportResourceType::InternalRenderTargetResource)
		? EDisplayClusterViewportResourceType::InputShaderResource
		: EDisplayClusterViewportResourceType::AdditionalTargetableResource;

	TArray<FRHITexture2D*> Input, Output;
	TArray<FIntRect> InputRects, OutputRects;
	if (!InSrcViewportProxy.GetResourcesWithRects_RenderThread(InSrcResourceType, Input, InputRects)
		|| !GetResourcesWithRects_RenderThread(DestResourceType, Output, OutputRects)
		|| Input.Num() != Output.Num())
	{
		return false;
	};

	FRDGBuilder GraphBuilder(RHICmdList);

	bool bResult = false;
	for (int32 ContextNum = 0; ContextNum < Input.Num(); ContextNum++)
	{
		const bool bUnpremultiply = EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard | EDisplayClusterViewportRuntimeICVFXFlags::Lightcard);
		const bool bInvertAlpha = !EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard);

		if (OpenColorIO->AddPass_RenderThread(
			GraphBuilder,
			InSrcViewportProxy.GetContexts_RenderThread()[ContextNum],
			Input[ContextNum], InputRects[ContextNum],
			Output[ContextNum], OutputRects[ContextNum],
			bUnpremultiply,
			bInvertAlpha))
		{
			bResult = true;
		}
	}

	if (bResult)
	{
		GraphBuilder.Execute();

		// copy OCIO results back
		if (DestResourceType != EDisplayClusterViewportResourceType::InputShaderResource)
		{
			ResolveResources_RenderThread(RHICmdList, DestResourceType, EDisplayClusterViewportResourceType::InputShaderResource);
		}
	}

	return bResult;
}

void FDisplayClusterViewportProxy::UpdateDeferredResources(FRHICommandListImmediate& RHICmdList) const
{
	using namespace UE::DisplayCluster::ViewportProxy;
	check(IsInRenderingThread());

	if (RenderSettings.bFreezeRendering || RenderSettings.bSkipRendering)
	{
		// Disable deferred update
		return;
	}

	if (RenderSettings.GetViewportOverrideMode() == EDisplayClusterViewportOverrideMode::All)
	{
		// Disable deferred update for clone viewports
		return;
	}

	const FDisplayClusterViewportProxy& SourceViewportProxy = GetRenderingViewportProxy();
	if(!SourceViewportProxy.IsInputRenderTargetResourceExists())
	{
		// No input RTT resource for deferred update
		return;
	}

	EDisplayClusterViewportResourceType SrcResourceType = EDisplayClusterViewportResourceType::InternalRenderTargetResource;

	// pre-Pass 0 (Projection policy):The projection policy can use its own method to resolve 'InternalRenderTargetResource' to 'InputShaderResource'
	if (ProjectionPolicy.IsValid() && ProjectionPolicy->ResolveInternalRenderTargetResource_RenderThread(RHICmdList, this, &SourceViewportProxy))
	{
		SrcResourceType = EDisplayClusterViewportResourceType::InputShaderResource;
	}

	// Pass 0 (OCIO): OCIO support on the first pass for an resolved RTT
	if(!ApplyOCIO_RenderThread(RHICmdList, SourceViewportProxy, SrcResourceType))
	{
		// Pass 0 (default): Resolve from RTT region to separated viewport context resource:
		if (SrcResourceType == EDisplayClusterViewportResourceType::InternalRenderTargetResource)
		{
			ImplResolveResources_RenderThread(RHICmdList, &SourceViewportProxy, SrcResourceType, EDisplayClusterViewportResourceType::InputShaderResource);
		}
	}

	// Pass 1: Generate blur postprocess effect for render target texture rect for all contexts
	if (PostRenderSettings.PostprocessBlur.IsEnabled())
	{
		TArray<FRHITexture2D*> InShaderResources;
		TArray<FRHITexture2D*> OutTargetableResources;
		if (GetResources_RenderThread(EDisplayClusterViewportResourceType::InputShaderResource, InShaderResources) && GetResources_RenderThread(EDisplayClusterViewportResourceType::AdditionalTargetableResource, OutTargetableResources))
		{
			// Render postprocess blur:
			for (int32 ContextNum = 0; ContextNum < InShaderResources.Num(); ContextNum++)
			{
				GetShadersAPI().RenderPostprocess_Blur(RHICmdList, InShaderResources[ContextNum], OutTargetableResources[ContextNum], PostRenderSettings.PostprocessBlur);
			}

			// Copy result back to input
			ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::AdditionalTargetableResource, EDisplayClusterViewportResourceType::InputShaderResource);
		}
	}

	// Pass 2: Create mips texture and generate mips from render target rect for all contexts
	if (PostRenderSettings.GenerateMips.IsEnabled())
	{
		TArray<FRHITexture2D*> InOutMipsResources;
		if (GetResources_RenderThread(EDisplayClusterViewportResourceType::MipsShaderResource, InOutMipsResources))
		{
			// Copy input image to layer0 on mips texture
			ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::InputShaderResource, EDisplayClusterViewportResourceType::MipsShaderResource);

			// Generate mips
			for (FRHITexture2D*& ResourceIt : InOutMipsResources)
			{
				GetShadersAPI().GenerateMips(RHICmdList, ResourceIt, PostRenderSettings.GenerateMips);
			}
		}
	}
}

FIntRect FDisplayClusterViewportProxy::GetFinalContextRect(const EDisplayClusterViewportResourceType InExtResourceType, const FIntRect& InRect) const
{
	const EDisplayClusterViewportResourceType InResourceType = GetResourceType_RenderThread(InExtResourceType);

	// When resolving without warp, apply overscan
	switch (InResourceType)
	{
	case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
		if (OverscanRuntimeSettings.bIsEnabled && CVarDisplayClusterRenderOverscanResolve.GetValueOnRenderThread() != 0)
		{
			// Support overscan crop
			return OverscanRuntimeSettings.OverscanPixels.GetInnerRect(InRect);
		}
		break;
	default:
		break;
	}

	return InRect;
}

// Resolve resource contexts
bool FDisplayClusterViewportProxy::ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const EDisplayClusterViewportResourceType InExtResourceType, const EDisplayClusterViewportResourceType OutExtResourceType, const int32 InContextNum) const
{
	return ImplResolveResources_RenderThread(RHICmdList, this, InExtResourceType, OutExtResourceType, InContextNum);
}

bool FDisplayClusterViewportProxy::ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, IDisplayClusterViewportProxy* InputResourceViewportProxy, const EDisplayClusterViewportResourceType InExtResourceType, const EDisplayClusterViewportResourceType OutExtResourceType, const int32 InContextNum) const
{
	const FDisplayClusterViewportProxy* SourceProxy = static_cast<FDisplayClusterViewportProxy*>(InputResourceViewportProxy);

	return ImplResolveResources_RenderThread(RHICmdList, SourceProxy , InExtResourceType, OutExtResourceType, InContextNum);
}

bool FDisplayClusterViewportProxy::ImplResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* SourceProxy, const EDisplayClusterViewportResourceType InExtResourceType, const EDisplayClusterViewportResourceType OutExtResourceType, const int32 InContextNum) const
{
	using namespace UE::DisplayCluster::ViewportProxy;

	check(IsInRenderingThread());
	check(SourceProxy);

	const EDisplayClusterViewportResourceType InResourceType = SourceProxy->GetResourceType_RenderThread(InExtResourceType);
	const EDisplayClusterViewportResourceType OutResourceType = GetResourceType_RenderThread(OutExtResourceType);

	if (InResourceType == EDisplayClusterViewportResourceType::MipsShaderResource)
	{
		// RenderTargetMips not allowved for resolve op
		return false;
	}

	const bool bOutputIsMipsResource    = OutResourceType == EDisplayClusterViewportResourceType::MipsShaderResource;
	const bool bOutputIsPreviewResource = OutResourceType == EDisplayClusterViewportResourceType::OutputPreviewTargetableResource;

	TArray<FViewportResourceResolverData> ResourceResolverData;

	TArray<FRHITexture2D*> SrcResources, DestResources;
	TArray<FIntRect> SrcResourcesRect, DestResourcesRect;
	if (SourceProxy->GetResourcesWithRects_RenderThread(InExtResourceType, SrcResources, SrcResourcesRect) && GetResourcesWithRects_RenderThread(OutExtResourceType, DestResources, DestResourcesRect))
	{
		const int32 SrcResourcesAmount = FMath::Min(SrcResources.Num(), DestResources.Num());
		for (int32 SrcResourceContextIndex = 0; SrcResourceContextIndex < SrcResourcesAmount; SrcResourceContextIndex++)
		{
			// Get context num to copy
			const int32 SrcContextNum = (InContextNum == INDEX_NONE) ? SrcResourceContextIndex : InContextNum;
			const FIntRect SrcRect = SourceProxy->GetFinalContextRect(InExtResourceType, SrcResourcesRect[SrcContextNum]);

			if ((SrcContextNum + 1) == SrcResourcesAmount)
			{
				// last input mono -> stereo outputs
				for (int32 DestResourceContextIndex = SrcContextNum; DestResourceContextIndex < DestResources.Num(); DestResourceContextIndex++)
				{
					const FIntRect DestRect = GetFinalContextRect(OutResourceType, DestResourcesRect[DestResourceContextIndex]);
					ResourceResolverData.Add(FViewportResourceResolverData(Contexts[SrcContextNum], SrcResources[SrcContextNum], SrcRect, DestResources[DestResourceContextIndex], DestRect, bOutputIsMipsResource, bOutputIsPreviewResource));
				}
				break;
			}
			else
			{
				const FIntRect DestRect = GetFinalContextRect(OutResourceType, DestResourcesRect[SrcContextNum]);
				ResourceResolverData.Add(FViewportResourceResolverData(Contexts[SrcContextNum], SrcResources[SrcContextNum], SrcRect, DestResources[SrcContextNum], DestRect, bOutputIsMipsResource, bOutputIsPreviewResource));
			}

			if (InContextNum != INDEX_NONE)
			{
				// Copy only one texture
				break;
			}
		}

		if (InExtResourceType == EDisplayClusterViewportResourceType::AfterWarpBlendTargetableResource
			&& OutExtResourceType == EDisplayClusterViewportResourceType::OutputTargetableResource
			&& DisplayDeviceProxy.IsValid()
			&& DisplayDeviceProxy->HasFinalPass_RenderThread())
		{
			// Custom resolve at external Display Device
			FRDGBuilder GraphBuilder(RHICmdList);
			for (FViewportResourceResolverData& ResourceResolverIt : ResourceResolverData)
			{
				ResourceResolverIt.AddFinalPass_RenderThread(GraphBuilder, *DisplayDeviceProxy.Get());
			}
			GraphBuilder.Execute();
		}
		else
		{
			// Standard resolve:
			for (FViewportResourceResolverData& ResourceResolverIt : ResourceResolverData)
			{
				ResourceResolverIt.Resolve_RenderThread(RHICmdList);
			}
		}

		return true;
	}

	return false;
}

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, const EDisplayClusterViewportResourceType InExtSrcResourceType, const EDisplayClusterViewportResourceType InExtDestResourceType)
{
	TArray<FRHITexture2D*> SrcResources;
	TArray<FIntRect> SrcResourceRects;
	if (GetResourcesWithRects_RenderThread(InExtSrcResourceType, SrcResources, SrcResourceRects))
	{
		check(SrcResources.IsValidIndex(InContextNum));
		check(SrcResourceRects.IsValidIndex(InContextNum));

		const FIntRect SrcRect = SrcResourceRects[InContextNum];

		FRDGTextureRef SrcTextureRef = RegisterExternalTexture(GraphBuilder, SrcResources[InContextNum]->GetTexture2D(), TEXT("DCViewportProxyCopySrcResource"));
		GraphBuilder.SetTextureAccessFinal(SrcTextureRef, ERHIAccess::SRVGraphics);

		return CopyResource_RenderThread(GraphBuilder, InCopyMode, InContextNum, SrcTextureRef, SrcRect, InExtDestResourceType);
	}

	return false;
}

BEGIN_SHADER_PARAMETER_STRUCT(FDisplayClusterCopyTextureParameters, )
RDG_TEXTURE_ACCESS(Input, ERHIAccess::CopySrc)
RDG_TEXTURE_ACCESS(Output, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, FRDGTextureRef InSrcTextureRef, const FIntRect& InSrcRect, const EDisplayClusterViewportResourceType InExtDestResourceType)
{
	using namespace UE::DisplayCluster::ViewportProxy;

	TArray<FRHITexture2D*> DestResources;
	TArray<FIntRect> DestResourceRects;
	if (HasBeenProduced(InSrcTextureRef) && GetResourcesWithRects_RenderThread(InExtDestResourceType, DestResources, DestResourceRects))
	{
		check(DestResources.IsValidIndex(InContextNum));
		check(DestResourceRects.IsValidIndex(InContextNum));

		FRDGTextureRef DestTextureRef = RegisterExternalTexture(GraphBuilder, DestResources[InContextNum]->GetTexture2D(), TEXT("DCViewportProxyCopyDestResource"));
		const FIntRect DestRect = DestResourceRects[InContextNum];

		FDisplayClusterCopyTextureParameters* PassParameters = GraphBuilder.AllocParameters<FDisplayClusterCopyTextureParameters>();
		PassParameters->Input = InSrcTextureRef;
		PassParameters->Output = DestTextureRef;
		GraphBuilder.SetTextureAccessFinal(DestTextureRef, ERHIAccess::RTV);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DisplayClusterViewportProxy_CopyResource(%s)", *GetId()),
			PassParameters,
			ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
			[InCopyMode, InContextNum, InSrcTextureRef, InSrcRect, DestTextureRef, DestRect](FRHICommandListImmediate& RHICmdList)
			{
				ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, InSrcTextureRef->GetRHI(), DestTextureRef->GetRHI(), InSrcRect, DestRect, InCopyMode);
			});

		return true;
	}

	return false;
}

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, const EDisplayClusterViewportResourceType InExtSrcResourceType, FRDGTextureRef InDestTextureRef, const FIntRect& InDestRect)
{
	using namespace UE::DisplayCluster::ViewportProxy;

	TArray<FRHITexture2D*> SrcResources;
	TArray<FIntRect> SrcResourceRects;
	if (HasBeenProduced(InDestTextureRef) && GetResourcesWithRects_RenderThread(InExtSrcResourceType, SrcResources, SrcResourceRects))
	{
		check(SrcResources.IsValidIndex(InContextNum));
		check(SrcResourceRects.IsValidIndex(InContextNum));

		FRDGTextureRef SrcTextureRef = RegisterExternalTexture(GraphBuilder, SrcResources[InContextNum]->GetTexture2D(), TEXT("DCViewportProxyCopySrcResource"));
		const FIntRect SrcRect = SrcResourceRects[InContextNum];

		FDisplayClusterCopyTextureParameters* PassParameters = GraphBuilder.AllocParameters<FDisplayClusterCopyTextureParameters>();
		PassParameters->Input = SrcTextureRef;
		PassParameters->Output = InDestTextureRef;
		GraphBuilder.SetTextureAccessFinal(SrcTextureRef, ERHIAccess::RTV);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DisplayClusterViewportProxy_CopyResource(%s)", *GetId()),
			PassParameters,
			ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
			[InCopyMode, InContextNum, SrcTextureRef, SrcRect, InDestTextureRef, InDestRect](FRHICommandListImmediate& RHICmdList)
			{
				ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, SrcTextureRef->GetRHI(), InDestTextureRef->GetRHI(), SrcRect, InDestRect, InCopyMode);
			});

		return true;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ShouldUseAlphaChannel_RenderThread() const
{
	// Chromakey and Light Cards use alpha channel
	if (EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::Chromakey))
	{
		return true;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ShouldUsePostProcessPassAfterSSRInput() const
{	
	if (ShouldUseAlphaChannel_RenderThread())
	{
		return ConfigurationProxy->GetRenderFrameSettings().AlphaChannelCaptureMode == EDisplayClusterRenderFrameAlphaChannelCaptureMode::ThroughTonemapper;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ShouldUsePostProcessPassAfterFXAA() const
{
	if (ShouldUseAlphaChannel_RenderThread())
	{
		return ConfigurationProxy->GetRenderFrameSettings().AlphaChannelCaptureMode == EDisplayClusterRenderFrameAlphaChannelCaptureMode::ThroughTonemapper;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ShouldUsePostProcessPassTonemap() const
{
	if(GetOpenColorIOMode() == EDisplayClusterViewportOpenColorIOMode::PostProcess)
	{
		return true;
	}

	return false;
}

void FDisplayClusterViewportProxy::OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, const FDisplayClusterViewportProxy_Context& InProxyContext)
{
	const uint32 InContextNum = InProxyContext.ContextNum;
	if (ShouldUseAlphaChannel_RenderThread())
	{
		switch (ConfigurationProxy->GetRenderFrameSettings().AlphaChannelCaptureMode)
		{
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::FXAA:
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::Copy:
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::CopyAA:
		{
			const FIntRect SrcRect = GetFinalContextRect(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Contexts[InContextNum].RenderTargetRect);
			// Copy alpha channel from 'SceneTextures.Color.Resolve' to 'InputShaderResource'
			CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, InContextNum, SceneTextures.Color.Resolve, SrcRect, EDisplayClusterViewportResourceType::InputShaderResource);
		}
		break;

		default:
			break;
		}
	}
}

FScreenPassTexture FDisplayClusterViewportProxy::OnPostProcessPassAfterSSRInput_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
{
	FScreenPassTexture OutScreenPassTexture = FDisplayClusterViewportManagerViewExtension::ReturnUntouchedSceneColorForPostProcessing(Inputs);
	if (OutScreenPassTexture.IsValid())
	{
		// Copy alpha channel to 'InputShaderResource'
		const FIntRect SrcRect = GetFinalContextRect(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Contexts[ContextNum].RenderTargetRect);
		CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, ContextNum, OutScreenPassTexture.Texture, SrcRect, EDisplayClusterViewportResourceType::InputShaderResource);
	}

	return OutScreenPassTexture;
}

FScreenPassTexture FDisplayClusterViewportProxy::OnPostProcessPassAfterFXAA_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
{
	FScreenPassTexture OutScreenPassTexture = FDisplayClusterViewportManagerViewExtension::ReturnUntouchedSceneColorForPostProcessing(Inputs);
	if (OutScreenPassTexture.IsValid())
	{
		// Restore alpha channel after OCIO
		// Copy alpha channel from 'InputShaderResource'
		const FIntRect DestRect = GetFinalContextRect(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Contexts[ContextNum].RenderTargetRect);
		CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, ContextNum, EDisplayClusterViewportResourceType::InputShaderResource, OutScreenPassTexture.Texture, DestRect);
	}

	return OutScreenPassTexture;
}

FScreenPassTexture FDisplayClusterViewportProxy::OnPostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
{
	// Perform OCIO rendering after the tonemapper
	if (GetOpenColorIOMode() == EDisplayClusterViewportOpenColorIOMode::PostProcess)
	{
		return OpenColorIO->PostProcessPassAfterTonemap_RenderThread(GraphBuilder, GetContexts_RenderThread()[ContextNum], View, Inputs);
	}

	return FDisplayClusterViewportManagerViewExtension::ReturnUntouchedSceneColorForPostProcessing(Inputs);
}

void FDisplayClusterViewportProxy::OnPostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily, const FSceneView& InSceneView, const FDisplayClusterViewportProxy_Context& InProxyContext)
{
	using namespace UE::DisplayCluster::ViewportProxy;

	const uint32 InContextNum = InProxyContext.ContextNum;

#if WITH_MGPU
	// Get the GPUIndex used to render this viewport
	if (Contexts.IsValidIndex(InContextNum))
	{

		const uint32 GPUIndex = InSceneView.GPUMask.GetFirstIndex();
		Contexts[InContextNum].RenderThreadData.GPUIndex = (GPUIndex < GNumExplicitGPUsForRendering) ? GPUIndex : -1;
	}
#endif

	if (Contexts.IsValidIndex(InContextNum))
	{
		Contexts[InContextNum].RenderThreadData.EngineDisplayGamma = InSceneView.Family->RenderTarget->GetDisplayGamma();
		Contexts[InContextNum].RenderThreadData.EngineShowFlags = InSceneView.Family->EngineShowFlags;
	}

	if (!InProxyContext.ViewFamilyProfileDescription.IsEmpty())
	{
		static IDisplayClusterCallbacks& DCCallbacksAPI = IDisplayCluster::Get().GetCallbacks();
		if (DCCallbacksAPI.OnDisplayClusterPostRenderViewFamily_RenderThread().IsBound())
		{
			// Now we can perform viewport notification
			DCCallbacksAPI.OnDisplayClusterPostRenderViewFamily_RenderThread().Broadcast(GraphBuilder, InViewFamily, this);
		}
	}

	if (ShouldUseAlphaChannel_RenderThread())
	{
		switch (ConfigurationProxy->GetRenderFrameSettings().AlphaChannelCaptureMode)
		{
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::FXAA:
		{
			// Restore alpha channed
			EFXAAQuality FXAAQuality = EFXAAQuality::Q0;
			if (GetFXAAQuality(RenderSettings.CaptureMode, FXAAQuality) && Resources[EDisplayClusterViewportResource::InputShaderResources].IsValidIndex(InContextNum))
			{
				if (FRHITexture2D* InputTextureRHI = Resources[EDisplayClusterViewportResource::InputShaderResources][InContextNum] ? Resources[EDisplayClusterViewportResource::InputShaderResources][InContextNum]->GetViewportResourceRHI_RenderThread() : nullptr)
				{
					// Apply FXAA for RGB only
					// Note: Add AA for alpha channel

					// Copy Alpha channels back from'InputShaderResource' to 'InternalRenderTargetResource'
					CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, InContextNum, EDisplayClusterViewportResourceType::InputShaderResource, EDisplayClusterViewportResourceType::InternalRenderTargetResource);

					// 1. Copy RGB channels from 'InternalRenderTargetResource' to 'InputShaderResource'
					CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::RGB, InContextNum, EDisplayClusterViewportResourceType::InternalRenderTargetResource, EDisplayClusterViewportResourceType::InputShaderResource);

					// 2. FXAA render pass to 'InputShaderResource'
					// Input is 'InputShaderResource'
					FRDGTextureRef InputTexture = RegisterExternalTexture(GraphBuilder, InputTextureRHI, TEXT("DCViewportProxyFXAAResource"));
					GraphBuilder.SetTextureAccessFinal(InputTexture, ERHIAccess::RTV);

					FFXAAInputs PassInputs;
					PassInputs.SceneColor = FScreenPassTexture(InputTexture);
					PassInputs.Quality = FXAAQuality;

					// 2.1. Do FXAA
					FScreenPassTexture OutputColorTexture = AddFXAAPass(GraphBuilder, InSceneView, PassInputs);

					// 2.2. Copy FXAA result from 'OutputTexture' to the 'InternalRenderTargetResource'
					if (OutputColorTexture.Texture)
					{
						const FIntVector OutputTextureSize = OutputColorTexture.Texture->Desc.GetSize();
						const FIntRect SrcRect(FIntPoint(0, 0), FIntPoint(OutputTextureSize.X, OutputTextureSize.Y));

						CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::RGB, InContextNum, OutputColorTexture.Texture, SrcRect, EDisplayClusterViewportResourceType::InternalRenderTargetResource);
					}
				}
				break;
			}
			// don't break (FXAA not used, just copy alpha)
		}

		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::Copy:
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::CopyAA:
			// Copy Alpha channels back from'InputShaderResource' to 'InternalRenderTargetResource'
			CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, InContextNum, EDisplayClusterViewportResourceType::InputShaderResource, EDisplayClusterViewportResourceType::InternalRenderTargetResource);
			break;

		default:
			break;
		}
	}
}

void FDisplayClusterViewportProxy::ReleaseTextures_RenderThread()
{
	Resources.ReleaseAllResources();
}
