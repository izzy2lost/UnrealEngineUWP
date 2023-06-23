// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceCullingOcclusionQuery.h"

#include "Containers/ArrayView.h"
#include "Containers/ResourceArray.h"
#include "GPUScene.h"
#include "GlobalShader.h"
#include "RHIAccess.h"
#include "RHIFeatureLevel.h"
#include "RHIGlobals.h"
#include "RHIShaderPlatform.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "HAL/IConsoleManager.h"
#include "DataDrivenShaderPlatformInfo.h"

static TAutoConsoleVariable<int32> CVarInstanceCullingOcclusionQueries(
	TEXT("r.InstanceCulling.OcclusionQueries"),
	0,
	TEXT("EXPERIMENTAL: Use per-instance software occlusion queries to perform less conservative visibility test than what's possible with HZB alone"),
	ECVF_RenderThreadSafe);

namespace
{

// YURIY_TODO: Put the helpers somewhere in the common RHI code
struct FResourceArrayView : public FResourceArrayInterface
{
	const void* Data = nullptr;
	uint32 SizeInBytes = 0;

	template <typename T>
	FResourceArrayView(TArrayView<T> View)
		: Data(View.GetData())
		, SizeInBytes(View.Num() * View.GetTypeSize())
	{}

	// FResourceArrayInterface
	virtual const void* GetResourceData() const override final { return Data; }
	virtual uint32 GetResourceDataSize() const override final { return SizeInBytes; }
	virtual void Discard() override final {};
	virtual bool IsStatic() const override final { return true; }
	virtual bool GetAllowCPUAccess() const override final { return false; };
	virtual void SetAllowCPUAccess(bool /*bInNeedsCPUAccess*/) override final {};
};

template <typename T>
static FBufferRHIRef CreateBufferWithData(FRHICommandListBase& RHICmdList, EBufferUsageFlags UsageFlags, ERHIAccess ResourceState, const TCHAR* Name, TConstArrayView<T> Data)
{
	FResourceArrayView DataView(Data);
	FRHIResourceCreateInfo CreateInfo(Name);
	CreateInfo.ResourceArray = &DataView;
	return RHICmdList.CreateBuffer(DataView.SizeInBytes, UsageFlags, Data.GetTypeSize(), ResourceState, CreateInfo);
}

}

/*
* Prepares indirect draw parameters for per-instance per-pixel occlusion query rendering pass.
*/
class FInstanceCullingOcclusionQueryCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInstanceCullingOcclusionQueryCS);
	SHADER_USE_PARAMETER_STRUCT(FInstanceCullingOcclusionQueryCS, FGlobalShader);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(Parameters.Platform);
	}

	static constexpr int32 NumThreadsPerGroup = 64;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, OutIndirectArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, OutInstanceIdBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutVisibilityMask) // One uint32 per instance (0 if instance is culled, non-0 otherwise)
		SHADER_PARAMETER(uint32, InstanceSceneDataSOAStride)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstanceSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstancePayloadData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
		SHADER_PARAMETER(FVector2f, HZBSize)
		SHADER_PARAMETER(FIntVector4, ViewRect)
		SHADER_PARAMETER(int32, NumInstances)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FInstanceCullingOcclusionQueryCS, "/Engine/Private/InstanceCulling/InstanceCullingOcclusionQuery.usf", "MainCS", SF_Compute);

class FInstanceCullingOcclusionQueryVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInstanceCullingOcclusionQueryVS);
	SHADER_USE_PARAMETER_STRUCT(FInstanceCullingOcclusionQueryVS, FGlobalShader);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectDrawArgsBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER(uint32, InstanceSceneDataSOAStride)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstanceSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstancePayloadData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, InstanceIdBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
		SHADER_PARAMETER(FVector2f, HZBSize)
		SHADER_PARAMETER(FIntVector4, ViewRect)
	END_SHADER_PARAMETER_STRUCT()
};

class FInstanceCullingOcclusionQueryPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInstanceCullingOcclusionQueryPS);
	SHADER_USE_PARAMETER_STRUCT(FInstanceCullingOcclusionQueryPS, FGlobalShader);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutVisibilityMask) // One uint32 per instance (0 if instance is culled, non-0 otherwise)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FInstanceCullingOcclusionQueryVS, "/Engine/Private/InstanceCulling/InstanceCullingOcclusionQuery.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FInstanceCullingOcclusionQueryPS, "/Engine/Private/InstanceCulling/InstanceCullingOcclusionQuery.usf", "MainPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FOcclusionInstanceCullingParameters,)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingOcclusionQueryVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingOcclusionQueryPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FInstanceCullingOcclusionQueryBox : public FRenderResource
{
public:
	FBufferRHIRef IndexBuffer;
	FBufferRHIRef VertexBuffer;
	FVertexDeclarationRHIRef VertexDeclaration;

	// Destructor
	virtual ~FInstanceCullingOcclusionQueryBox() {}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		static const uint16 BoxIndexBufferData[] =
		{
			0, 1, 2, 0, 2, 3,
			4, 5, 6, 4, 6, 7,
			1, 4, 7, 1, 7, 2,
			5, 0, 3, 5, 3, 6,
			5, 4, 1, 5, 1, 0,
			3, 2, 7, 3, 7, 6,
		};

		static const FVector3f BoxVertexBufferData[] =
		{
			FVector3f(-1.0f, +1.0f, +1.0f),
			FVector3f(+1.0f, +1.0f, +1.0f),
			FVector3f(+1.0f, -1.0f, +1.0f),
			FVector3f(-1.0f, -1.0f, +1.0f),
			FVector3f(+1.0f, +1.0f, -1.0f),
			FVector3f(-1.0f, +1.0f, -1.0f),
			FVector3f(-1.0f, -1.0f, -1.0f),
			FVector3f(+1.0f, -1.0f, -1.0f),
		};

		IndexBuffer = CreateBufferWithData(RHICmdList, EBufferUsageFlags::IndexBuffer, ERHIAccess::VertexOrIndexBuffer,
			TEXT("FInstanceCullingOcclusionQueryBox_IndexBuffer"),
			MakeArrayView(BoxIndexBufferData));

		VertexBuffer = CreateBufferWithData(RHICmdList, EBufferUsageFlags::VertexBuffer, ERHIAccess::VertexOrIndexBuffer,
			TEXT("FInstanceCullingOcclusionQueryBox_VertexBuffer"),
			MakeArrayView(BoxVertexBufferData));

		FVertexDeclarationElementList VertexDeclarationElements;
		VertexDeclarationElements.Add(FVertexElement(0, 0, VET_Float3, 0, 12));
		VertexDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(VertexDeclarationElements);
	}

	virtual void ReleaseRHI() override
	{
		IndexBuffer.SafeRelease();
		VertexBuffer.SafeRelease();
		VertexDeclaration.SafeRelease();
	}
};

TGlobalResource<FInstanceCullingOcclusionQueryBox> GInstanceCullingOcclusionQueryBox;

static void RenderInstanceOcclusionCulling(
	FRHICommandList& RHICmdList,
	FViewInfo& View,
	FOcclusionInstanceCullingParameters* PassParameters,
	int32 NumInstances)
{
	TShaderMapRef<FInstanceCullingOcclusionQueryVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FInstanceCullingOcclusionQueryPS> PixelShader(View.ShaderMap);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	FIntVector4 ViewRect = PassParameters->VS.ViewRect;
	RHICmdList.SetViewport(ViewRect.X, ViewRect.Y, 0.0f, ViewRect.Z, ViewRect.W, 1.0f);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GInstanceCullingOcclusionQueryBox.VertexDeclaration;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI(); // Depth test, no write
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI(); // Blend state does not matter, as we are not writing to render targets
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	ClearUnusedGraphResources(VertexShader, &PassParameters->VS);
	ClearUnusedGraphResources(PixelShader, &PassParameters->PS);

	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

	RHICmdList.SetStreamSource(0, GInstanceCullingOcclusionQueryBox.VertexBuffer, 0);

	FRDGBufferRef IndirectArgsBuffer = PassParameters->VS.IndirectDrawArgsBuffer;
	IndirectArgsBuffer->MarkResourceAsUsed();

	RHICmdList.DrawIndexedPrimitiveIndirect(GInstanceCullingOcclusionQueryBox.IndexBuffer, IndirectArgsBuffer->GetRHI(), 0);
}

uint32 FInstanceCullingOcclusionQueryRenderer::Render(
	FRDGBuilder& GraphBuilder,
	FGPUScene& GPUScene,
	FViewInfo& View)
{
	if (!IsCompatibleWithView(View))
	{
		return 0 ;
	}

	const uint32 ViewMask = RegisterView(View);

	if (ViewMask == 0)
	{
		// Silently fall back to no culling when we hit the limit of maximum supported views
		return 0;
	}

	FRDGTextureRef DepthTexture = View.GetSceneTextures().Depth.Target;
	FRDGTextureRef HZBTexture = View.HZB;

	checkf(DepthTexture && HZBTexture,
		TEXT("Occlusion query instance culling pass requires scene depth texture and HZB. See FInstanceCullingOcclusionQueryRenderer::IsCompatibleWithView()"));

	const FIntVector HZBSize = HZBTexture->Desc.GetSize();

	const FGPUSceneResourceParameters GPUSceneParameters = GPUScene.GetShaderParameters();

	const int32 NumInstances = GPUScene.GetNumInstances();
	const FIntPoint ViewRectSize = View.ViewRect.Size();

	const FIntVector NumThreadGroups = FComputeShaderUtils::GetGroupCount(NumInstances, FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup);
	const int32 MaxSupportedInstances = GRHIGlobals.MaxDispatchThreadGroupsPerDimension.X * FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup;
	if (!ensureMsgf(NumThreadGroups.X * FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup <= MaxSupportedInstances,
		TEXT("Number of instances (%d) is greater than currently supported by FInstanceCullingOcclusionQueryRenderer (%d). ")
		TEXT("Per-instance occlusion queries will be disabled. ")
		TEXT("Increase FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup or implement wrapped group count support."),
		NumInstances, MaxSupportedInstances))
	{
		return 0;
	}

	// Align buffer sizes to ensure each thread in the thread group has a valid slot to write without introducing bounds checks
	const int32 AlignedNumInstances = NumThreadGroups.X * FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup;

	// Create the result buffer on demand
	if (!CurrentInstanceOcclusionQueryBuffer)
	{
		CurrentInstanceOcclusionQueryBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), AlignedNumInstances),
			TEXT("FInstanceCullingOcclusionQueryRenderer_VisibleInstanceMask"));

		AllocatedNumInstances = NumInstances;
	}

	checkf(uint32(NumInstances) == AllocatedNumInstances, TEXT("Number of instances in GPUScene is not expected change to during the frame"));

	FRDGBufferRef VisibleInstanceMaskBuffer = CurrentInstanceOcclusionQueryBuffer;
	FRDGBufferUAVRef VisibilityMaskUAV = GraphBuilder.CreateUAV(VisibleInstanceMaskBuffer, PF_R32_UINT);

	FRDGBufferRef IndirectArgsBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndexedIndirectParameters>(1),
		TEXT("FInstanceCullingOcclusionQueryRenderer_IndirectArgsBuffer"));
	FRDGBufferUAVRef IndirectArgsUAV = GraphBuilder.CreateUAV(IndirectArgsBuffer, PF_R32_UINT);

	FRDGBufferRef InstanceIdBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), AlignedNumInstances),
		TEXT("FInstanceCullingOcclusionQueryRenderer_InstanceIdBuffer"));
	FRDGBufferUAVRef InstanceIdUAV = GraphBuilder.CreateUAV(InstanceIdBuffer, PF_R32_UINT);
	FRDGBufferSRVRef InstanceIdSRV = GraphBuilder.CreateSRV(InstanceIdBuffer, PF_R32_UINT);

	AddClearUAVPass(GraphBuilder, IndirectArgsUAV, 0);

	// Compute pass to perform initial per-instance filtering and prepare instance list for per-pixel occlusion tests
	{
		FInstanceCullingOcclusionQueryCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInstanceCullingOcclusionQueryCS::FParameters>();

		PassParameters->OutIndirectArgsBuffer = IndirectArgsUAV;
		PassParameters->OutInstanceIdBuffer = InstanceIdUAV;
		PassParameters->OutVisibilityMask = VisibilityMaskUAV;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->HZBTexture = HZBTexture;
		PassParameters->HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->HZBSize = FVector2f(HZBSize.X, HZBSize.Y);
		PassParameters->ViewRect = FIntVector4(View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Max.X, View.ViewRect.Max.Y);
		PassParameters->InstanceSceneDataSOAStride = GPUSceneParameters.InstanceDataSOAStride;
		PassParameters->GPUSceneInstanceSceneData = GPUSceneParameters.GPUSceneInstanceSceneData;
		PassParameters->GPUSceneInstancePayloadData = GPUSceneParameters.GPUSceneInstancePayloadData;
		PassParameters->GPUScenePrimitiveSceneData = GPUSceneParameters.GPUScenePrimitiveSceneData;
		PassParameters->NumInstances = NumInstances;

		TShaderMapRef<FInstanceCullingOcclusionQueryCS> ComputeShader(View.ShaderMap);

		ClearUnusedGraphResources(ComputeShader, PassParameters);

		FComputeShaderUtils::AddPass(
			GraphBuilder, 
			RDG_EVENT_NAME("InstanceCullingOcclusionQueryRenderer_Setup"),
			ComputeShader,
			PassParameters,
			NumThreadGroups
		);
	}

	// Perform per-instance per-pixel occlusion tests by drawing bounding boxes that write into VisibleInstanceMaskBuffer slots for visible instances
	{
		FOcclusionInstanceCullingParameters* PassParameters = GraphBuilder.AllocParameters<FOcclusionInstanceCullingParameters>();

		PassParameters->VS.IndirectDrawArgsBuffer = IndirectArgsBuffer;
		PassParameters->VS.View = View.ViewUniformBuffer;
		PassParameters->VS.HZBTexture = HZBTexture;
		PassParameters->VS.HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->VS.HZBSize = FVector2f(HZBSize.X, HZBSize.Y);
		PassParameters->VS.ViewRect = FIntVector4(View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Max.X, View.ViewRect.Max.Y);
		PassParameters->VS.InstanceSceneDataSOAStride = GPUSceneParameters.InstanceDataSOAStride;
		PassParameters->VS.GPUSceneInstanceSceneData = GPUSceneParameters.GPUSceneInstanceSceneData;
		PassParameters->VS.GPUSceneInstancePayloadData = GPUSceneParameters.GPUSceneInstancePayloadData;
		PassParameters->VS.GPUScenePrimitiveSceneData = GPUSceneParameters.GPUScenePrimitiveSceneData;
		PassParameters->VS.InstanceIdBuffer = InstanceIdSRV;
		PassParameters->PS.OutVisibilityMask = VisibilityMaskUAV;
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(DepthTexture,
			ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ENoAction,
			FExclusiveDepthStencil::DepthRead_StencilNop);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("InstanceCullingOcclusionQueryRenderer_Draw"),
			PassParameters, ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
			[PassParameters, NumInstances, &View](FRHICommandList& RHICmdList)
			{
				RenderInstanceOcclusionCulling(RHICmdList, View, PassParameters, NumInstances);
			});
	}

	return ViewMask;
}

void FInstanceCullingOcclusionQueryRenderer::EndFrame(FRDGBuilder& GraphBuilder)
{
	if (CurrentInstanceOcclusionQueryBuffer)
	{
		GraphBuilder.QueueBufferExtraction(CurrentInstanceOcclusionQueryBuffer, &InstanceOcclusionQueryBuffer, ERHIAccess::SRVMask);
		CurrentInstanceOcclusionQueryBuffer = {};
		AllocatedNumInstances = 0;
	}
	CurrentRenderedViewIDs.Empty();
}

uint32 FInstanceCullingOcclusionQueryRenderer::RegisterView(FViewInfo& View)
{
	if (CurrentRenderedViewIDs.Num() < MaxViews)
	{
		int32 Index = CurrentRenderedViewIDs.AddUnique(View.GetViewKey());
		check(Index >= 0 && Index < MaxViews);
		return 1u << Index;
	}
	else
	{
		return 0;
	}
}

bool FInstanceCullingOcclusionQueryRenderer::IsCompatibleWithView(FViewInfo& View)
{
	return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(View.GetShaderPlatform())
		&& View.GetSceneTextures().Depth.Target
		&& View.HZB
		&& CVarInstanceCullingOcclusionQueries.GetValueOnRenderThread() != 0;
}
