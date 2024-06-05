// Copyright Epic Games, Inc. All Rights Reserved.

#include "MobileSSR.h"

#include "RenderGraph.h"
#include "PixelShaderUtils.h"
#include "ScreenPass.h"
#include "PostProcess/SceneRenderTargets.h"
#include "ScenePrivate.h"
#include "SystemTextures.h"

int32 GMobileScreenSpaceReflectionsSupported = 0;
static FAutoConsoleVariableRef CVarMobileScreenSpaceReflections(
	TEXT("r.Mobile.ScreenSpaceReflections"),
	GMobileScreenSpaceReflectionsSupported,
	TEXT("0: Mobile Renderer Screen Space Reflections disabled (default)\n"
		 "1: Mobile Renderer Screen Space Reflections enabled\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

bool ShouldRenderMobileSSR(const FViewInfo& View)
{
	return GMobileScreenSpaceReflectionsSupported && ScreenSpaceRayTracing::ShouldRenderScreenSpaceReflections(View) && View.PrevViewInfo.TemporalAAHistory.RT[0] && View.HZB;
}

bool IsMobileSSREnabled(const FViewInfo& View)
{
	return GMobileScreenSpaceReflectionsSupported && ScreenSpaceRayTracing::ShouldRenderScreenSpaceReflections(View);
}

class FMobileSSRQualityDim : SHADER_PERMUTATION_ENUM_CLASS("SSR_QUALITY", ESSRQuality);
class FMobileSSRUseVelocity : SHADER_PERMUTATION_BOOL("SSR_USE_VELOCITY");

class FMobileScreenSpaceReflectionsPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMobileScreenSpaceReflectionsPS);
	SHADER_USE_PARAMETER_STRUCT(FMobileScreenSpaceReflectionsPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<FMobileSSRQualityDim, FMobileSSRUseVelocity>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePlatform(Parameters.Platform) && IsMobileDeferredShadingEnabled(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SUPPORTS_ANISOTROPIC_MATERIALS"), FDataDrivenShaderPlatformInfo::GetSupportsAnisotropicMaterials(Parameters.Platform));

		check(IsMobileDeferredShadingEnabled(Parameters.Platform));
		OutEnvironment.SetDefine(TEXT("ENABLE_SHADINGMODEL_SUPPORT_MOBILE_DEFERRED"), MobileUsesGBufferCustomData(Parameters.Platform));
		OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEFERREDSHADING_SUBPASS"), 1u);
		OutEnvironment.SetDefine(TEXT("FORCE_DEPTH_TEXTURE_READS"), 1u);
		
		OutEnvironment.SetDefine(TEXT("SSR_OUTPUT_FOR_DENOISER"), 0u);
		OutEnvironment.SetDefine(TEXT("DIM_LIGHTING_TERM"), 0u);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FLinearColor, SSRParams)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FMobileScreenSpaceReflectionsPS, "/Engine/Private/SSRT/SSRTReflections.usf", "MobileScreenSpaceReflectionsPS", SF_Pixel);


using DepthStencilStateMobileSSR = TStaticDepthStencilState<
	false, CF_Always,
	true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
	false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
	STENCIL_MOBILE_REFLECTIVE_MASK | STENCIL_MOBILE_SKY_MASK, 0x00>;

const uint8 StencilRefMobileSSR = STENCIL_MOBILE_REFLECTIVE_MASK; // Process SSR where there are potentially reflective materials (and not sky)

void SetupMobileSSRParameters(FRDGBuilder& GraphBuilder,const FViewInfo& View, FMobileScreenSpaceReflectionParams& Params)
{
	if (!ShouldRenderMobileSSR(View))
	{
		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		Params.HZB = SystemTextures.Black;
		Params.HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Params.SceneColor = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(SystemTextures.Black));
		Params.SceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Params.PrevSceneColorBilinearUVMinMax = FVector4f(0.0f, 0.0f, 1.0f, 1.0f);
		Params.QualityAndExposureCorrection = FVector4f(ForceInitToZero);
		return;
	}
	FRDGTextureRef SceneColor = GraphBuilder.RegisterExternalTexture(View.PrevViewInfo.TemporalAAHistory.RT[0]);
	Params.SceneColor = GraphBuilder.CreateSRV(SceneColor->Desc.IsTextureArray()
		? FRDGTextureSRVDesc::CreateForSlice(SceneColor, View.PrevViewInfo.TemporalAAHistory.OutputSliceIndex)
		: FRDGTextureSRVDesc(SceneColor));
	Params.SceneColorSampler = TStaticSamplerState<SF_Point>::GetRHI();
	Params.HZB = View.HZB;
	Params.HZBSampler = TStaticSamplerState<SF_Point>::GetRHI();

	{
		const FVector2D HZBUvFactor(
			float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
			float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y));
		Params.HZBUvFactorAndInvFactor = FVector4f(
			HZBUvFactor.X,
			HZBUvFactor.Y,
			1.0f / HZBUvFactor.X,
			1.0f / HZBUvFactor.Y);
	}

	{
		ensure(View.PrevViewInfo.TemporalAAHistory.IsValid());
		FIntPoint ViewportOffset = View.PrevViewInfo.TemporalAAHistory.ViewportRect.Min;
		FIntPoint ViewportExtent = View.PrevViewInfo.TemporalAAHistory.ViewportRect.Size();
		FIntPoint BufferSize = View.PrevViewInfo.TemporalAAHistory.ReferenceBufferSize;
		ensure(ViewportExtent.X > 0 && ViewportExtent.Y > 0);
		ensure(BufferSize.X > 0 && BufferSize.Y > 0);

		FVector2D InvBufferSize(1.0f / float(BufferSize.X), 1.0f / float(BufferSize.Y));
		Params.PrevScreenPositionScaleBias = FVector4f(
			ViewportExtent.X * 0.5f * InvBufferSize.X,
			-ViewportExtent.Y * 0.5f * InvBufferSize.Y,
			(ViewportExtent.X * 0.5f + ViewportOffset.X) * InvBufferSize.X,
			(ViewportExtent.Y * 0.5f + ViewportOffset.Y) * InvBufferSize.Y);
	}

	{
		ESSRQuality SSRQuality;
		IScreenSpaceDenoiser::FReflectionsRayTracingConfig DenoiserConfig;
		ScreenSpaceRayTracing::GetSSRQualityForView(View, &SSRQuality, &DenoiserConfig);

		Params.QualityAndExposureCorrection.X = (float)SSRQuality;
		Params.QualityAndExposureCorrection.Y = 1.0f / View.PrevViewInfo.SceneColorPreExposure;
		Params.QualityAndExposureCorrection.Z = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
		Params.QualityAndExposureCorrection.W = FMath::Clamp(View.FinalPostProcessSettings.ScreenSpaceReflectionMaxRoughness, 0.0f, 1.0f);
	}

	{
		FScreenPassTextureViewportParameters PrevSceneColorParameters = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(SceneColor->Desc.Extent, View.PrevViewInfo.TemporalAAHistory.ViewportRect));
		Params.PrevSceneColorBilinearUVMinMax.X = PrevSceneColorParameters.UVViewportBilinearMin.X;
		Params.PrevSceneColorBilinearUVMinMax.Y = PrevSceneColorParameters.UVViewportBilinearMin.Y;
		Params.PrevSceneColorBilinearUVMinMax.Z = PrevSceneColorParameters.UVViewportBilinearMax.X;
		Params.PrevSceneColorBilinearUVMinMax.W = PrevSceneColorParameters.UVViewportBilinearMax.Y;
	}
}

void FMobileSceneRenderer::RenderSSR(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	if (!ShouldRenderMobileSSR(View))
	{
		return;
	}

	FMobileScreenSpaceReflectionsPS::FParameters Parameters;

	ESSRQuality SSRQuality;
	IScreenSpaceDenoiser::FReflectionsRayTracingConfig DenoiserConfig;
	ScreenSpaceRayTracing::GetSSRQualityForView(View, &SSRQuality, &DenoiserConfig);
	if (SSRQuality < ESSRQuality::Low)
	{
		return;
	}

	Parameters.SSRParams = ScreenSpaceRayTracing::ComputeSSRParams(View, SSRQuality, false);

	Parameters.ViewUniformBuffer = View.ViewUniformBuffer;

	FMobileScreenSpaceReflectionsPS::FPermutationDomain PermutationVectorPS;
	PermutationVectorPS.Set<FMobileSSRQualityDim>(SSRQuality);
	PermutationVectorPS.Set<FMobileSSRUseVelocity>(bShouldRenderVelocities);

	TShaderMapRef<FMobileScreenSpaceReflectionsPS> PixelShader(View.ShaderMap, PermutationVectorPS);
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	FPixelShaderUtils::InitFullscreenPipelineState(RHICmdList, View.ShaderMap, PixelShader, /* out */ GraphicsPSOInit);

	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.DepthStencilState = DepthStencilStateMobileSSR::GetRHI();

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRefMobileSSR);
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Parameters);

	FPixelShaderUtils::DrawFullscreenTriangle(RHICmdList);
}
