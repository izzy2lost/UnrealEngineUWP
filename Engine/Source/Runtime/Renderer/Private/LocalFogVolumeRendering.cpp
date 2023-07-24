// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalFogVolumeRendering.h"
#include "ScenePrivate.h"
#include "RendererUtils.h"
#include "ScreenPass.h"
#include "LocalFogVolumeSceneProxy.h"
#include "MobileBasePassRendering.h"


// The runtime ON/OFF toggle
static TAutoConsoleVariable<int32> CVarLocalFogVolume(
	TEXT("r.LocalFogVolume"), 1,
	TEXT("LocalFogVolume components are rendered when this is not 0, otherwise ignored.\n"),
	ECVF_RenderThreadSafe);

bool ShouldRenderLocalFogVolume(const FScene* Scene, const FSceneViewFamily& Family)
{
	const FEngineShowFlags EngineShowFlags = Family.EngineShowFlags;
	if (Scene && Scene->HasAnyLocalFogVolume() && EngineShowFlags.Fog && !Family.UseDebugViewPS())
	{
		return CVarLocalFogVolume.GetValueOnRenderThread() > 0;
	}
	return false;
}

DECLARE_GPU_STAT(LocalFogVolumeVolumes);

/*=============================================================================
	FScene functions
=============================================================================*/

void FScene::AddLocalFogVolume(class FLocalFogVolumeSceneProxy* FogProxy)
{
	check(FogProxy);
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FAddLocalFogVolumeCommand)(
		[Scene, FogProxy](FRHICommandListImmediate& RHICmdList)
		{
			check(!Scene->LocalFogVolumes.Contains(FogProxy));
			Scene->LocalFogVolumes.Push(FogProxy);
		} );
}

void FScene::RemoveLocalFogVolume(class FLocalFogVolumeSceneProxy* FogProxy)
{
	check(FogProxy);
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FRemoveLocalFogVolumeCommand)(
		[Scene, FogProxy](FRHICommandListImmediate& RHICmdList)
		{
			Scene->LocalFogVolumes.RemoveSingle(FogProxy);
		} );
}

bool FScene::HasAnyLocalFogVolume() const
{ 
	return LocalFogVolumes.Num() > 0;
}

/*=============================================================================
	Local height fog rendering common function
=============================================================================*/

void GetLocalFogVolumeSortingData(const FScene* Scene, FRDGBuilder& GraphBuilder, FLocalFogVolumeSortingData& Out)
{
	// No culling as of today
	Out.LocalFogVolumeInstanceCount = Scene->LocalFogVolumes.Num();
	Out.LocalFogVolumeInstanceCountFinal = 0;
	Out.LocalFogVolumeGPUInstanceData = (FLocalFogVolumeGPUInstanceData*)GraphBuilder.Alloc(sizeof(FLocalFogVolumeGPUInstanceData) * Out.LocalFogVolumeInstanceCount, 16);
	Out.LocalFogVolumeCenterPos = (FVector*)GraphBuilder.Alloc(sizeof(FVector3f) * Out.LocalFogVolumeInstanceCount, 16);
	Out.LocalFogVolumeSortKeys.SetNumUninitialized(Out.LocalFogVolumeInstanceCount);
	for (FLocalFogVolumeSceneProxy* LHF : Scene->LocalFogVolumes)
	{
		if (LHF->FogDensity <= 0.0f)
		{
			continue; // this volume will never be visible
		}

		FTransform TransformScaleOnly;
		TransformScaleOnly.SetScale3D(LHF->FogTransform.GetScale3D());

		FLocalFogVolumeGPUInstanceData* LocalFogVolumeGPUInstanceDataIt = &Out.LocalFogVolumeGPUInstanceData[Out.LocalFogVolumeInstanceCountFinal];
		LocalFogVolumeGPUInstanceDataIt->Transform = FMatrix44f(LHF->FogTransform.ToMatrixWithScale());
		LocalFogVolumeGPUInstanceDataIt->InvTransform = LocalFogVolumeGPUInstanceDataIt->Transform.Inverse();
		LocalFogVolumeGPUInstanceDataIt->InvTranformNoScale = FMatrix44f(LHF->FogTransform.ToMatrixNoScale()).Inverse();
		LocalFogVolumeGPUInstanceDataIt->TransformScaleOnly = FMatrix44f(TransformScaleOnly.ToMatrixWithScale());

		LocalFogVolumeGPUInstanceDataIt->Density = LHF->FogDensity;
		LocalFogVolumeGPUInstanceDataIt->HeightFalloff = LHF->FogHeightFalloff * 0.01f;	// This scale is used to have artist author reasonable range.
		LocalFogVolumeGPUInstanceDataIt->HeightOffset = LHF->FogHeightOffset;
		LocalFogVolumeGPUInstanceDataIt->RadialAttenuation = LHF->FogRadialAttenuation;

		LocalFogVolumeGPUInstanceDataIt->FogMode = float(LHF->FogMode);

		LocalFogVolumeGPUInstanceDataIt->Albedo = FVector3f(LHF->FogAlbedo);
		LocalFogVolumeGPUInstanceDataIt->PhaseG = LHF->FogPhaseG;
		LocalFogVolumeGPUInstanceDataIt->Emissive = FVector3f(LHF->FogEmissive);

		Out.LocalFogVolumeCenterPos[Out.LocalFogVolumeInstanceCountFinal] = LHF->FogTransform.GetTranslation();

		FLocalFogVolumeSortKey* LocalFogVolumeSortKeysIt = &Out.LocalFogVolumeSortKeys[Out.LocalFogVolumeInstanceCountFinal];
		LocalFogVolumeSortKeysIt->FogVolume.Index = Out.LocalFogVolumeInstanceCountFinal;
		LocalFogVolumeSortKeysIt->FogVolume.Distance = 0;	// Filled up right before sorting according to a view
		LocalFogVolumeSortKeysIt->FogVolume.Priority = LHF->FogSortPriority;

		Out.LocalFogVolumeInstanceCountFinal++;
	}
	// Shrink the array to only what is needed in order for the sort to correctly work on only what is needed.
	Out.LocalFogVolumeSortKeys.SetNum(Out.LocalFogVolumeInstanceCountFinal, false/*bAllowShrinking*/);
}

void CreateViewLocalFogVolumeBufferSRV(FViewInfo& View, FRDGBuilder& GraphBuilder, FLocalFogVolumeSortingData& SortingData)
{
	static const uint32 SizeOfFloat4 = sizeof(float) * 4;
	static const uint32 Float4CountInLocalFogVolumeGPUInstanceData = sizeof(FLocalFogVolumeGPUInstanceData) / SizeOfFloat4;
	static_assert(sizeof(FLocalFogVolumeGPUInstanceData) == Float4CountInLocalFogVolumeGPUInstanceData * SizeOfFloat4); // The size of the structure must be a multiple of FVector4.

	if (SortingData.LocalFogVolumeInstanceCountFinal == 0)
	{
		View.LocalFogVolumeGPUInstanceCount = 0;

		static FLocalFogVolumeGPUInstanceData DummyData;
		View.LocalFogVolumeGPUInstanceDataBufferSRV = GraphBuilder.CreateSRV(
			CreateVertexBuffer(GraphBuilder, TEXT("LocalFogVolumeGPUInstanceDataBuffer"), 
			FRDGBufferDesc::CreateBufferDesc(SizeOfFloat4, Float4CountInLocalFogVolumeGPUInstanceData), &DummyData, sizeof(FLocalFogVolumeGPUInstanceData) * 1, ERDGInitialDataFlags::NoCopy),
			PF_A32B32G32R32F);
		return;
	}

	// 1. Sort all the volumes
	const FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();
	for (uint32 i = 0; i < SortingData.LocalFogVolumeInstanceCountFinal; ++i)
	{
		FVector FogCenterPos = SortingData.LocalFogVolumeCenterPos[SortingData.LocalFogVolumeSortKeys[i].FogVolume.Index];	// Recovered form the original array via index because the sorting of the previous view might have changed the order.
		float DistancetoView = float((FogCenterPos - ViewOrigin).Size());
		SortingData.LocalFogVolumeSortKeys[i].FogVolume.Distance = *reinterpret_cast<uint32*>(&DistancetoView);
	}
	SortingData.LocalFogVolumeSortKeys.Sort();

	// 2. Create the buffer containing all the fog volume data instance sorted according to their key for the current view.
	FLocalFogVolumeGPUInstanceData* LocalFogVolumeGPUSortedInstanceData = (FLocalFogVolumeGPUInstanceData*)GraphBuilder.Alloc(sizeof(FLocalFogVolumeGPUInstanceData) * SortingData.LocalFogVolumeInstanceCountFinal, 16);
	for (uint32 i = 0; i < SortingData.LocalFogVolumeInstanceCountFinal; ++i)
	{
		// We could also have an indirection buffer on GPU but choosing to go with the sorting + copy on CPU since it is expected to not have many local height fog volumes.
		LocalFogVolumeGPUSortedInstanceData[i] = SortingData.LocalFogVolumeGPUInstanceData[SortingData.LocalFogVolumeSortKeys[i].FogVolume.Index];
	}

	// 3. Allocate buffer and initialize with sorted data to upload to GPU
	const uint32 AllLocalFogVolumeInstanceBytesFinal = sizeof(FLocalFogVolumeGPUInstanceData) * SortingData.LocalFogVolumeInstanceCountFinal;
	FRDGBufferRef LocalFogVolumeGPUInstanceDataBuffer = CreateVertexBuffer(
		GraphBuilder, TEXT("LocalFogVolumeGPUInstanceDataBuffer"),
		FRDGBufferDesc::CreateBufferDesc(SizeOfFloat4, SortingData.LocalFogVolumeInstanceCountFinal * Float4CountInLocalFogVolumeGPUInstanceData), 
		LocalFogVolumeGPUSortedInstanceData, AllLocalFogVolumeInstanceBytesFinal, ERDGInitialDataFlags::NoCopy);

	View.LocalFogVolumeGPUInstanceCount = SortingData.LocalFogVolumeInstanceCountFinal;
	View.LocalFogVolumeGPUInstanceDataBufferSRV = GraphBuilder.CreateSRV(LocalFogVolumeGPUInstanceDataBuffer, PF_A32B32G32R32F);
}

/*=============================================================================
	Local height fog rendering - non mobile
=============================================================================*/

class FLocalFogVolumeVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalFogVolumeVS);
	SHADER_USE_PARAMETER_STRUCT(FLocalFogVolumeVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalFogVolumeInstances)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		return true;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalFogVolumeVS, "/Engine/Private/LocalFogVolume.usf", "LocalFogVolumeSplatVS", SF_Vertex);

class FLocalFogVolumePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalFogVolumePS);
	SHADER_USE_PARAMETER_STRUCT(FLocalFogVolumePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalFogVolumeInstances)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		return true;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalFogVolumePS, "/Engine/Private/LocalFogVolume.usf", "LocalFogVolumeSplatPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FLocalFogVolumePassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FLocalFogVolumeVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLocalFogVolumePS::FParameters, PS)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()
 
void RenderLocalFogVolume(
	const FScene* Scene,
	TArray<FViewInfo>& Views,
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FRDGTextureRef LightShaftOcclusionTexture)
{
	uint32 LocalFogVolumeInstanceCount = Scene->LocalFogVolumes.Num();
	if (LocalFogVolumeInstanceCount > 0)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, LocalFogVolumeVolumes);

		FLocalFogVolumeSortingData SortingData;
		GetLocalFogVolumeSortingData(Scene, GraphBuilder, SortingData);

		if (SortingData.LocalFogVolumeInstanceCountFinal > 0)
		{
			FRDGTextureRef SceneColorTexture = SceneTextures.Color.Resolve;

			for (FViewInfo& View : Views)
			{
				CreateViewLocalFogVolumeBufferSRV(View, GraphBuilder, SortingData);
				if (View.LocalFogVolumeGPUInstanceCount == 0)
				{
					continue;
				}

				FLocalFogVolumePassParameters* PassParameters = GraphBuilder.AllocParameters<FLocalFogVolumePassParameters>();

				PassParameters->VS.View = GetShaderBinding(View.ViewUniformBuffer);
				PassParameters->VS.LocalFogVolumeInstances = View.LocalFogVolumeGPUInstanceDataBufferSRV;

				PassParameters->PS.View = GetShaderBinding(View.ViewUniformBuffer);
				PassParameters->PS.LocalFogVolumeInstances = View.LocalFogVolumeGPUInstanceDataBufferSRV;

				PassParameters->SceneTextures = SceneTextures.UniformBuffer;
				PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ENoAction);

				FLocalFogVolumeVS::FPermutationDomain VSPermutationVector;
				auto VertexShader = View.ShaderMap->GetShader< FLocalFogVolumeVS >(VSPermutationVector);

				FLocalFogVolumePS::FPermutationDomain PsPermutationVector;
				auto PixelShader = View.ShaderMap->GetShader< FLocalFogVolumePS >(PsPermutationVector);

				const FIntRect ViewRect = View.ViewRect;

				ClearUnusedGraphResources(VertexShader, &PassParameters->VS);
				ClearUnusedGraphResources(PixelShader, &PassParameters->PS);

				uint32 LocalFogVolumeGPUInstanceCount = View.LocalFogVolumeGPUInstanceCount;
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("RenderLocalFogVolume %u inst.", LocalFogVolumeGPUInstanceCount),
					PassParameters,
					ERDGPassFlags::Raster,
					[VertexShader, PixelShader, PassParameters, LocalFogVolumeGPUInstanceCount, ViewRect](FRHICommandList& RHICmdList)
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

					// Render back faces only since camera may intersect
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

					RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);

					RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer()
						, 0									//BaseVertexIndex
						, 0									//FirstInstance
						, 8									//uint32 NumVertices
						, 0									//uint32 StartIndex
						, UE_ARRAY_COUNT(GCubeIndices) / 3	//uint32 NumPrimitives
						, LocalFogVolumeGPUInstanceCount	//uint32 NumInstances
					);
				});
			}
		}
	}
}

/*=============================================================================
	Local height fog rendering - mobile
=============================================================================*/
	

class FMobileLocalFogVolumeVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMobileLocalFogVolumeVS);
	SHADER_USE_PARAMETER_STRUCT(FMobileLocalFogVolumeVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalFogVolumeInstances)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		const bool bMobileForceDepthRead = MobileUsesFullDepthPrepass(Parameters.Platform);
		OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEPTHREAD_SUBPASS"), bMobileForceDepthRead ? 0u : 1u);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileLocalFogVolumeVS, "/Engine/Private/LocalFogVolume.usf", "LocalFogVolumeSplatVS", SF_Vertex);

class FMobileLocalFogVolumePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMobileLocalFogVolumePS);
	SHADER_USE_PARAMETER_STRUCT(FMobileLocalFogVolumePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileBasePassUniformParameters, MobileBasePass)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalFogVolumeInstances)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		const bool bMobileForceDepthRead = MobileUsesFullDepthPrepass(Parameters.Platform);
		OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEPTHREAD_SUBPASS"), bMobileForceDepthRead ? 0u : 1u);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileLocalFogVolumePS, "/Engine/Private/LocalFogVolume.usf", "LocalFogVolumeSplatPS", SF_Pixel);

void RenderLocalFogVolumeMobile(
	FRHICommandList& RHICmdList,
	const FViewInfo& View)
{
	if (View.LocalFogVolumeGPUInstanceCount == 0)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, LocalFogVolumeVolumes);

	FMobileLocalFogVolumeVS::FPermutationDomain VSPermutationVector;
	auto VertexShader = View.ShaderMap->GetShader< FMobileLocalFogVolumeVS >(VSPermutationVector);

	FMobileLocalFogVolumePS::FPermutationDomain PsPermutationVector;
	auto PixelShader = View.ShaderMap->GetShader< FMobileLocalFogVolumePS >(PsPermutationVector);

	const FIntRect ViewRect = View.ViewRect;

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

	// Render back faces only since camera may intersect
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	FMobileLocalFogVolumeVS::FParameters VSParameters;
	VSParameters.View = View.GetShaderParameters();
	VSParameters.LocalFogVolumeInstances = View.LocalFogVolumeGPUInstanceDataBufferSRV;
	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VSParameters);

	FMobileLocalFogVolumePS::FParameters PSParameters;
	PSParameters.View = View.GetShaderParameters();
	PSParameters.LocalFogVolumeInstances = View.LocalFogVolumeGPUInstanceDataBufferSRV;
	// PSParameters.MobileBasePass filled up by the RDG pass parameters.
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PSParameters);

	RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);

	RHICmdList.DrawIndexedPrimitive(
		GetUnitCubeIndexBuffer()
		, 0										//BaseVertexIndex
		, 0										//FirstInstance
		, 8										//uint32 NumVertices
		, 0										//uint32 StartIndex
		, UE_ARRAY_COUNT(GCubeIndices) / 3		//uint32 NumPrimitives
		, View.LocalFogVolumeGPUInstanceCount	//uint32 NumInstances
	);
}
