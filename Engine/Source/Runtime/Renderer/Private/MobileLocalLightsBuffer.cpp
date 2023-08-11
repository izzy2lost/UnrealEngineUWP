// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "BasePassRendering.h"
#include "PixelShaderUtils.h"
#include "MobileBasePassRendering.h"
#include "RendererPrivateUtils.h"
#include "GlobalRenderResources.h"
#include "ScenePrivate.h"

bool MobileLocalLighsBufferEnabled(const FStaticShaderPlatform Platform)
{
	return !IsMobileDeferredShadingEnabled(Platform) && 
			IsMobilePlatform(Platform) && 
			MobileLocalLightsBufferEnabled(Platform);
}

const int32 GLocalLightPrepassTileSizeX = 8;
class FLocalLightBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightBufferCS);
public:
	SHADER_USE_PARAMETER_STRUCT(FLocalLightBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int32>, RWTileInfo)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER(FIntPoint, GroupSize)
	END_SHADER_PARAMETER_STRUCT()
 
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MobileLocalLighsBufferEnabled(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GLocalLightPrepassTileSizeX);
		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightBufferCS, "/Engine/Private/MobileLocalLightsBuffer.usf", "MainCS", SF_Compute);

class FLocalLightBufferVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightBufferVS);
	SHADER_USE_PARAMETER_STRUCT(FLocalLightBufferVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int32>, TileInfo)
		SHADER_PARAMETER(int32, LightGridPixelSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MobileLocalLighsBufferEnabled(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightBufferVS, "/Engine/Private/MobileLocalLightsBuffer.usf", "MainVS", SF_Vertex);

class FLocalLightBufferPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightBufferPS);
	SHADER_USE_PARAMETER_STRUCT(FLocalLightBufferPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return MobileLocalLighsBufferEnabled(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);

		if (MobileLocalLightsBufferPrepassEnabled(Parameters.Platform))
		{
			OutEnvironment.SetRenderTargetOutputFormat(0, PF_FloatR11G11B10);
			OutEnvironment.SetRenderTargetOutputFormat(1, PF_A2B10G10R10);
			OutEnvironment.SetDefine(TEXT("POST_PROCESS_LOCAL_LIGHTS"), 0);
		}
		else
		{
			OutEnvironment.SetRenderTargetOutputFormat(0, PF_FloatR11G11B10);
			OutEnvironment.SetDefine(TEXT("POST_PROCESS_LOCAL_LIGHTS"), 1);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightBufferPS, "/Engine/Private/MobileLocalLightsBuffer.usf", "Main", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FLocalLightBufferPrepassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FLocalLightBufferVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FLocalLightBufferPS::FParameters, PS)
SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void FMobileSceneRenderer::RenderMobileLocalLightsBuffer(FRDGBuilder& GraphBuilder, FSceneTextures& SceneTextures, bool bIsPrepass)
{
	if (!MobileLocalLighsBufferEnabled(ShaderPlatform) || 
		(bIsPrepass != MobileLocalLightsBufferPrepassEnabled(ShaderPlatform)) || 
		IsMobileDeferredShadingEnabled(ShaderPlatform))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "RenderMobileLocalLightsBuffer");
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderMobileLocalLightsBuffer);

	static const auto LightGridPixelSizeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Forward.LightGridPixelSize"));
	check(LightGridPixelSizeCVar != nullptr);
	int32 LightGridPixelSize = LightGridPixelSizeCVar->GetInt();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		bool bHasNoLocalLights = (!View.ForwardLightingResources.ForwardLightData) || (View.ForwardLightingResources.ForwardLightData->NumLocalLights == 0);
		if (!View.ShouldRenderView() || bHasNoLocalLights)
		{
			continue;
		}

		const FIntPoint GroupSize(
			FMath::DivideAndRoundUp(View.ViewRect.Size().X, LightGridPixelSize),
			FMath::DivideAndRoundUp(View.ViewRect.Size().Y, LightGridPixelSize));

		FRDGBufferRef TileInfoBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 2 * GroupSize.X * GroupSize.Y), TEXT("TileInfoBuffer"));
		FRDGBufferUAVRef TileInfoBufferUAV = GraphBuilder.CreateUAV(TileInfoBuffer, PF_R32_SINT);
		FRDGBufferSRVRef TileInfoBufferSRV = GraphBuilder.CreateSRV(TileInfoBuffer, PF_R32_SINT);

		{
			auto* PassParameters = GraphBuilder.AllocParameters<FLocalLightBufferCS::FParameters>();
			PassParameters->RWTileInfo = TileInfoBufferUAV;
			PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
			PassParameters->GroupSize = GroupSize;
			auto ComputeShader = View.ShaderMap->GetShader<FLocalLightBufferCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("RenderMobileLocalLights_TiledInfoCS"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FIntVector(FMath::DivideAndRoundUp<uint32>(GroupSize.Y * GroupSize.X, GLocalLightPrepassTileSizeX), 1, 1));
		}


		{
			FLocalLightBufferPrepassParameters* PassParameters = GraphBuilder.AllocParameters<FLocalLightBufferPrepassParameters>();
			if (bIsPrepass)
			{
				PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.MobileLocalLightTextureA, ERenderTargetLoadAction::EClear);
				PassParameters->RenderTargets[1] = FRenderTargetBinding(SceneTextures.MobileLocalLightTextureB, ERenderTargetLoadAction::EClear);
			}
			else
			{
				PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Resolve, ERenderTargetLoadAction::ELoad);
			}
			PassParameters->SceneTextures = SceneTextures.GetSceneTextureShaderParameters(View.FeatureLevel);

			PassParameters->VS.View = GetShaderBinding(View.ViewUniformBuffer);
			PassParameters->VS.TileInfo = TileInfoBufferSRV;
			PassParameters->VS.LightGridPixelSize = LightGridPixelSize;
			PassParameters->PS.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
			PassParameters->PS.View = GetShaderBinding(View.ViewUniformBuffer);

			auto VertexShader = View.ShaderMap->GetShader<FLocalLightBufferVS>();
			auto PixelShader = View.ShaderMap->GetShader<FLocalLightBufferPS>();

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RenderMobileLocalLightsBuffer %s", bIsPrepass ? TEXT("Prepass") : TEXT("PostProcess")),
				PassParameters,
				ERDGPassFlags::Raster,
				[PassParameters, VertexShader, PixelShader, &View, GroupSize, bIsPrepass](FRHICommandList& RHICmdList)
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					if (bIsPrepass)
					{
						GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					}
					else
					{
						GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI();
					}

					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GTileVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

					RHICmdList.SetStreamSource(0, GetOneTileQuadVertexBuffer(), 0);
					RHICmdList.DrawIndexedPrimitive(GetOneTileQuadIndexBuffer(),
						0,
						0,
						4,
						0,
						2,
						GroupSize.X * GroupSize.Y);
				});
		}
	}
}