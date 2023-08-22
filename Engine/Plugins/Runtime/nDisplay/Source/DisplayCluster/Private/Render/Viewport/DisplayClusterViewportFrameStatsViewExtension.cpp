// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportFrameStatsViewExtension.h"
#include "DisplayClusterSceneViewExtensions.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"

#include "GlobalShader.h"
#include "Misc/App.h"
#include "Misc/Timecode.h"
#include "SceneRendering.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"
#include "SystemTextures.h"
#include "PostProcess/PostProcessTestImage.h"
#include "PostProcess/PostProcessMaterialInputs.h"


int32 GDisplayClusterShowFrameStats = 0;
static FAutoConsoleVariableRef CVarDisplayClusterShowFrameStats(
	TEXT("DC.Stats.Frame"),
	GDisplayClusterShowFrameStats,
	TEXT("Show frame stats (timecode, frame number) in a repeating pattern."),
	ECVF_RenderThreadSafe
);

BEGIN_SHADER_PARAMETER_STRUCT(FDisplayClusterFrameStatsShaderParameters, )
	SHADER_PARAMETER_TEXTURE(Texture2D, MiniFontTexture)
	SHADER_PARAMETER(uint32, FrameCount)
	SHADER_PARAMETER(uint32, Timecode)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FDisplayClusterFrameStatsShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDisplayClusterFrameStatsShader);
	SHADER_USE_PARAMETER_STRUCT(FDisplayClusterFrameStatsShader, FGlobalShader);

	using FParameters = FDisplayClusterFrameStatsShaderParameters;
};

IMPLEMENT_SHADER_TYPE(, FDisplayClusterFrameStatsShader, TEXT("/Plugin/nDisplay/Private/FrameStats.usf"), TEXT("MainPS"), SF_Pixel);


BEGIN_SHADER_PARAMETER_STRUCT(FDisplayClusterFrameStatsOutputShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FrameStatsTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, FrameStatsTextureSampler)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FDisplayClusterFrameStatsOutputShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDisplayClusterFrameStatsOutputShader);
	SHADER_USE_PARAMETER_STRUCT(FDisplayClusterFrameStatsOutputShader, FGlobalShader);

	using FParameters = FDisplayClusterFrameStatsOutputShaderParameters;
};

IMPLEMENT_SHADER_TYPE(, FDisplayClusterFrameStatsOutputShader, TEXT("/Plugin/nDisplay/Private/FrameStats.usf"), TEXT("MainOutputPS"), SF_Pixel);

namespace UE::DisplayClusterViewExtension
{
	static FRHITexture* GetMiniFontTexture()
	{
		return GSystemTextures.AsciiTexture ? GSystemTextures.AsciiTexture->GetRHI() : GSystemTextures.WhiteDummy->GetRHI();
	}
}


///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportFrameStatsViewExtension
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportFrameStatsViewExtension::FDisplayClusterViewportFrameStatsViewExtension(const FAutoRegister& AutoRegister, const FDisplayClusterViewportManager* InViewportManager)
	: FSceneViewExtensionBase(AutoRegister)
	, ViewportManagerWeakPtr(InViewportManager->AsShared())
{ }

FDisplayClusterViewportFrameStatsViewExtension::~FDisplayClusterViewportFrameStatsViewExtension()
{
	ViewportManagerWeakPtr.Reset();
}

void FDisplayClusterViewportFrameStatsViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (PassId == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FDisplayClusterViewportFrameStatsViewExtension::PostProcessPassAfterTonemap_RenderThread));
	}
}

void FDisplayClusterViewportFrameStatsViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	const FTimecode CurrentTimecode = FApp::GetTimecode();

	EncodedTimecode = (static_cast<uint8>(CurrentTimecode.Hours) << 24u) | (static_cast<uint8>(CurrentTimecode.Minutes) << 16u) | (static_cast<uint8>(CurrentTimecode.Seconds) << 8u) | static_cast<uint8>(CurrentTimecode.Frames);
	FrameCount = GFrameCounter;
}

FScreenPassTexture FDisplayClusterViewportFrameStatsViewExtension::PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	const FScreenPassTexture& SceneColor = Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
	check(SceneColor.IsValid());

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);

	FRDGTextureDesc TextureFrameStatsDesc = FRDGTextureDesc::Create2D(FIntPoint(128, 64), PF_R8G8B8A8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable);
	FRDGTexture* TextureFrameStats = GraphBuilder.CreateTexture(TextureFrameStatsDesc, TEXT("DisplayCluster.FrameStatsRT"));

	// Render frame stats to a small render target to minimize shader costs
	// Note: As a further optimization, this could be renderered once per view family.
	{
		const FScreenPassTextureViewport Viewport(TextureFrameStats);
		FRenderTargetBinding FrameStatsRenderTargetBinding(TextureFrameStats, ERenderTargetLoadAction::EClear);

		TShaderMapRef<FDisplayClusterFrameStatsShader> FrameStatsPixelShader(ShaderMap);
		FDisplayClusterFrameStatsShaderParameters* Parameters = GraphBuilder.AllocParameters<FDisplayClusterFrameStatsShaderParameters>();
		Parameters->MiniFontTexture = UE::DisplayClusterViewExtension::GetMiniFontTexture();
		Parameters->RenderTargets[0] = FrameStatsRenderTargetBinding;
		Parameters->FrameCount = FrameCount;
		Parameters->Timecode = EncodedTimecode;

		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("DisplayClusterViewportFrameStatsPass"), ViewInfo, Viewport, Viewport, VertexShader, FrameStatsPixelShader, Parameters);
	}

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, SceneColor, View.GetOverwriteLoadAction(), TEXT("DisplayCluster.FrameStatsOutput"));
	}

	// Render output composited with frame stats texture
	{
		const FScreenPassTextureViewport InputViewport(SceneColor);
		const FScreenPassTextureViewport OutputViewport(Output);

		TShaderMapRef<FDisplayClusterFrameStatsOutputShader> OutputPixelShader(ShaderMap);
		FDisplayClusterFrameStatsOutputShaderParameters* Parameters = GraphBuilder.AllocParameters<FDisplayClusterFrameStatsOutputShaderParameters>();
		Parameters->InputTexture = SceneColor.Texture;
		Parameters->InputTextureSampler = TStaticSamplerState<>::GetRHI();
		Parameters->FrameStatsTexture = TextureFrameStats;
		Parameters->FrameStatsTextureSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap>::GetRHI();
		Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("DisplayClusterViewportFrameStatsOutputPass"), ViewInfo, OutputViewport, InputViewport, VertexShader, OutputPixelShader, Parameters);
	}

	return Output;
}

bool FDisplayClusterViewportFrameStatsViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	if (IsActive())
	{
		static const FDisplayClusterSceneViewExtensionContext DCViewExtensionContext;
		if (Context.IsA(MoveTempIfPossible(DCViewExtensionContext)))
		{
			const FDisplayClusterSceneViewExtensionContext& DisplayContext = static_cast<const FDisplayClusterSceneViewExtensionContext&>(Context);
			if (DisplayContext.GetViewportManager() == GetViewportManager())
			{
				// Apply only for DC viewports
				return true;
			}
		}
	}

	return false;
}

bool FDisplayClusterViewportFrameStatsViewExtension::IsActive() const
{
	return GetViewportManager() != nullptr && GDisplayClusterShowFrameStats > 0;
}
