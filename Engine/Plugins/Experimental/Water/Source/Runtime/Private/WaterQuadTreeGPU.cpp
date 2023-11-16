// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterQuadTreeGPU.h"

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "RenderUtils.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHIStaticStates.h"
#include "RHIFeatureLevel.h"
#include "PipelineStateCache.h"
#include "PixelShaderUtils.h"
#include "SystemTextures.h"
#include "ShaderPrintParameters.h"
#include "SceneRendering.h"

DECLARE_GPU_STAT(FWaterQuadTreeGPU_Init);
DECLARE_GPU_STAT(FWaterQuadTreeGPU_Traverse);

int32 GWaterQuadTreeParallelPrefixSum = 1;
static FAutoConsoleVariableRef CVarWaterQuadTreeParallelPrefixSum(
	TEXT("r.Water.WaterMesh.GPUQuadTree.ParallelPrefixSum"),
	GWaterQuadTreeParallelPrefixSum,
	TEXT("Enables using a parallel prefix sum algorithm for the water quadtree indirect draw call pipeline."),
	ECVF_RenderThreadSafe);

float GWaterQuadTreeZBoundsPadding = 200.0f;
static FAutoConsoleVariableRef CVarWaterQuadTreeZBoundsPadding(
	TEXT("r.Water.WaterMesh.GPUQuadTree.ZBoundsPadding"),
	GWaterQuadTreeZBoundsPadding,
	TEXT("Amount of padding to apply to the Z bounds of each quadtree node."
		" Necessary for sloped rivers, whose complete Z bounds can be underestimated due to using non-conservative rasterization to build the quadtree."),
	ECVF_RenderThreadSafe);

int32 GWaterQuadTreeOcclusionCulling = 1;
static FAutoConsoleVariableRef CVarWaterQuadTreeOcclusionCulling(
	TEXT("r.Water.WaterMesh.GPUQuadTree.OcclusionCulling"),
	GWaterQuadTreeOcclusionCulling,
	TEXT("Enables HZB occlusion culling for the water quadtree indirect draw call pipeline."),
	ECVF_RenderThreadSafe);

class FWaterQuadTreeVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreeVS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreeVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, Transform)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreeVS, "/Plugin/Water/Private/WaterQuadTreeVertexShader.usf", "Main", SF_Vertex);

class FWaterQuadTreePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreePS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, WaterBodyRenderDataIndex)
		SHADER_PARAMETER(int32, Priority)
		SHADER_PARAMETER(float, WaterBodyMinZ)
		SHADER_PARAMETER(float, WaterBodyMaxZ)
		SHADER_PARAMETER(float, MaxWaveHeight)
		SHADER_PARAMETER(uint32, bIsRiver)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreePS, "/Plugin/Water/Private/WaterQuadTreePixelShader.usf", "Main", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FWaterQuadTreeParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FWaterQuadTreeVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FWaterQuadTreePS::FParameters, PS)
END_SHADER_PARAMETER_STRUCT()

class FWaterQuadTreeMergePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreeMergePS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreeMergePS, FGlobalShader);

	class FNumMSAASamples : SHADER_PERMUTATION_SPARSE_INT("NUM_MSAA_SAMPLES", 1, 2, 4, 8);

	using FPermutationDomain = TShaderPermutationDomain<FNumMSAASamples>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DMS, WaterBodyRasterTextureMS)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DMS, ZBoundsRasterTextureMS)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, WaterBodyRasterTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ZBoundsRasterTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, WaterBodyRenderData)
		SHADER_PARAMETER(int32, SuperSamplingFactor)
		SHADER_PARAMETER(float, RcpCaptureDepthRange)
		SHADER_PARAMETER(float, ZBoundsPadding)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreeMergePS, "/Plugin/Water/Private/WaterQuadTreeMerge.usf", "Main", SF_Pixel);

class FWaterQuadTreeBuildPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreeBuildPS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreeBuildPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, QuadTreeTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, WaterZBoundsTexture)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreeBuildPS, "/Plugin/Water/Private/WaterQuadTreeBuild.usf", "Main", SF_Pixel);

class FWaterQuadTreeInitializeIndirectArgsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreeInitializeIndirectArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreeInitializeIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, IndirectArgs)
		SHADER_PARAMETER(uint32, NumDensities)
		SHADER_PARAMETER(uint32, NumMaterials)
		SHADER_PARAMETER(uint32, NumViews)
		SHADER_PARAMETER(uint32, NumQuadsLOD0)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INITIALIZE_INDIRECT_ARGS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreeInitializeIndirectArgsCS, "/Plugin/Water/Private/WaterQuadTreeDraws.usf", "MainCS", SF_Compute);

class FWaterQuadTreeTraverseCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreeTraverseCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreeTraverseCS, FGlobalShader);

	class FDebugShaderPrint : SHADER_PERMUTATION_BOOL("DEBUG_SHADER_PRINT");

	using FPermutationDomain = TShaderPermutationDomain<FDebugShaderPrint>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, PackedNodes)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, QuadTreeTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, WaterZBoundsTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, WaterBodyRenderData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
		SHADER_PARAMETER(FMatrix44f, TranslatedWorldToClip)
		SHADER_PARAMETER(FMatrix44f, ViewToClip)
		SHADER_PARAMETER(FVector4f, CullingBoundsAABB)
		SHADER_PARAMETER(FVector2f, HZBSize)
		SHADER_PARAMETER(FVector2f, HZBViewSize)
		SHADER_PARAMETER(FVector3f, QuadTreePosition)
		SHADER_PARAMETER(FVector3f, ObserverPosition)
		SHADER_PARAMETER(uint32, QuadTreeResolutionX)
		SHADER_PARAMETER(uint32, QuadTreeResolutionY)
		SHADER_PARAMETER(uint32, NumDensities)
		SHADER_PARAMETER(uint32, NumMaterials)
		SHADER_PARAMETER(float, LeafSize)
		SHADER_PARAMETER(float, LODScale)
		SHADER_PARAMETER(float, CaptureDepthRange)
		SHADER_PARAMETER(int32, ForceCollapseDensityLevel)
		SHADER_PARAMETER(uint32, NumLODs)
		SHADER_PARAMETER(uint32, NumDispatchedThreads)
		SHADER_PARAMETER(int32, DebugShowTile)
		SHADER_PARAMETER(uint32, bHZBOcclusionCullingEnabled)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const bool bDebugShaderPrint = PermutationVector.Get<FDebugShaderPrint>() != 0;
		return !bDebugShaderPrint || ShaderPrint::IsSupported(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		const bool bDebugShaderPrint = PermutationVector.Get<FDebugShaderPrint>() != 0;
		if (bDebugShaderPrint)
		{
			ShaderPrint::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		}
		OutEnvironment.SetDefine(TEXT("QUAD_TREE_TRAVERSE"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreeTraverseCS, "/Plugin/Water/Private/WaterQuadTreeDraws.usf", "MainCS", SF_Compute);

class FWaterQuadTreeBucketCountsCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreeBucketCountsCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreeBucketCountsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, BucketCounts)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, QuadTreeTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, WaterBodyRenderData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, PackedNodes)
		SHADER_PARAMETER(uint32, NumDensities)
		SHADER_PARAMETER(uint32, NumMaterials)
		SHADER_PARAMETER(uint32, NumDispatchedThreads)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPUTE_BUCKET_COUNTS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreeBucketCountsCS, "/Plugin/Water/Private/WaterQuadTreeDraws.usf", "MainCS", SF_Compute);

class FWaterQuadTreeBucketPrefixSumCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreeBucketPrefixSumCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreeBucketPrefixSumCS, FGlobalShader);

	class FParallelPrefixSum : SHADER_PERMUTATION_BOOL("PARALLEL_PREFIX_SUM");
	using FPermutationDomain = TShaderPermutationDomain<FParallelPrefixSum>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, BucketPrefixSums)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BucketCounts)
		SHADER_PARAMETER(uint32, NumBuckets)
		SHADER_PARAMETER(uint32, OutputOffset)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPUTE_BUCKET_PREFIX_SUM"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreeBucketPrefixSumCS, "/Plugin/Water/Private/WaterQuadTreeDraws.usf", "MainCS", SF_Compute);

class FWaterQuadTreeGenerateInstanceDataCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FWaterQuadTreeGenerateInstanceDataCS);
	SHADER_USE_PARAMETER_STRUCT(FWaterQuadTreeGenerateInstanceDataCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, InstanceData0)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, InstanceData1)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, InstanceData2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, QuadTreeTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, WaterZBoundsTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, WaterBodyRenderData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, PackedNodes)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, InstanceDataOffsets)
		SHADER_PARAMETER(FVector3f, QuadTreePosition)
		SHADER_PARAMETER(FVector3f, ObserverPosition)
		SHADER_PARAMETER(uint32, QuadTreeResolutionX)
		SHADER_PARAMETER(uint32, QuadTreeResolutionY)
		SHADER_PARAMETER(uint32, NumDensities)
		SHADER_PARAMETER(uint32, NumMaterials)
		SHADER_PARAMETER(uint32, NumDispatchedThreads)
		SHADER_PARAMETER(uint32, BucketIndexOffset)
		SHADER_PARAMETER(uint32, NumLODs)
		SHADER_PARAMETER(float, LeafSize)
		SHADER_PARAMETER(float, LODScale)
		SHADER_PARAMETER(float, CaptureDepthRange)
		SHADER_PARAMETER(uint32, bWithWaterSelectionSupport)
		SHADER_PARAMETER(uint32, bLODMorphingEnabled)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GENERATE_INSTANCE_DATA"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FWaterQuadTreeGenerateInstanceDataCS, "/Plugin/Water/Private/WaterQuadTreeDraws.usf", "MainCS", SF_Compute);


void FWaterQuadTreeGPU::Init(FRDGBuilder& GraphBuilder, const FInitParams& Params, TArray<FDraw>& Draws)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWaterQuadTreeGPU::Init);
	RDG_EVENT_SCOPE(GraphBuilder, "FWaterQuadTreeGPU::Init");
	RDG_GPU_STAT_SCOPE(GraphBuilder, FWaterQuadTreeGPU_Init);

	// Create resources
	{
		check(!bInitialized);
		check(!QuadTreeTexture);
		check(!WaterZBoundsTexture);
		check(!WaterBodyRenderDataBuffer);

		// Make sure the quadtree is a power of two
		const FIntPoint ResolutionRaw = Params.RequestedQuadTreeResolution;
		const FIntPoint ResolutionPow2 = FIntPoint(FMath::RoundUpToPowerOfTwo(ResolutionRaw.X), FMath::RoundUpToPowerOfTwo(ResolutionRaw.Y));
		const int32 MaxDim = (int32)FMath::Max(ResolutionPow2.X, ResolutionPow2.Y);
		const int32 NumMipLevels = (int32)FMath::FloorLog2(MaxDim) + 1;

		FRHITextureCreateDesc QuadTreeTextureCreateDesc = FRHITextureCreateDesc::Create2D(TEXT("WaterQuadTree.QuadTree"), ResolutionPow2, PF_B8G8R8A8)
			.SetNumMips(NumMipLevels)
			.SetFlags(TexCreate_RenderTargetable | TexCreate_ShaderResource);
		QuadTreeTexture = RHICreateTexture(QuadTreeTextureCreateDesc);

		FRHITextureCreateDesc ZBoundsTextureCreateDesc = FRHITextureCreateDesc::Create2D(TEXT("WaterQuadTree.ZBoundsTexture"), ResolutionPow2, PF_A2B10G10R10)
			.SetNumMips(NumMipLevels)
			.SetFlags(TexCreate_RenderTargetable | TexCreate_ShaderResource);
		WaterZBoundsTexture = RHICreateTexture(ZBoundsTextureCreateDesc);

		WaterBodyRenderDataBuffer = GraphBuilder.ConvertToExternalBuffer(CreateStructuredBuffer<FWaterBodyRenderDataGPU>(GraphBuilder, TEXT("WaterQuadTree.WaterBodyRenderData"), Params.WaterBodyRenderData));

		check(Params.CaptureDepthRange > 0.0f);
		CaptureDepthRange = Params.CaptureDepthRange;
		bInitialized = true;
	}

	auto GetNumMSAASamples = [](int32 RequestedNumSamples)
	{
		if (RequestedNumSamples <= 1)
		{
			return 1;
		}
		else if (RequestedNumSamples < 4)
		{
			return 2;
		}
		else if (RequestedNumSamples < 8)
		{
			return 4;
		}
		else
		{
			return 8;
		}
	};
	

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	FRDGTexture* QuadTreeTextureRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(QuadTreeTexture, TEXT("WaterQuadTree.QuadTree")));
	FRDGTexture* WaterZBoundsTextureRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(WaterZBoundsTexture, TEXT("WaterQuadTree.ZBoundsTexture")));

	FRDGBuffer* WaterBodyRenderDataBufferRDG = GraphBuilder.RegisterExternalBuffer(WaterBodyRenderDataBuffer);
	FRDGBufferSRV* WaterBodyRenderDataBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(WaterBodyRenderDataBufferRDG));

	const FIntPoint QuadTreeResolution = QuadTreeTextureRDG->Desc.Extent;
	// Not necessarily a power of two or a multiple of the quadtree LOD0 resolution. We rely on Texture.Load() to return 0 for out of bounds accesses.
	// Padding this texture is not required as we will not need to create a mip chain for it; but rather use it to initialize mip0 of our quadtree texture.
	const FIntPoint RasterResolution = Params.RequestedQuadTreeResolution * Params.SuperSamplingFactor;
	const int32 NumMipLevels = QuadTreeTextureRDG->Desc.NumMips;
	const uint8 NumSamples = GetNumMSAASamples(Params.NumMSAASamples);

	FRDGTextureDesc WaterBodyRasterTextureDesc = FRDGTextureDesc::Create2D(RasterResolution, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource, 1, NumSamples);
	FRDGTexture* WaterBodyRasterTexture = GraphBuilder.CreateTexture(WaterBodyRasterTextureDesc, TEXT("WaterQuadTree.WaterBodyRasterTexture"));
	FRDGTextureDesc ZBoundsRasterTextureDesc = FRDGTextureDesc::Create2D(RasterResolution, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource, 1, NumSamples);
	FRDGTexture* ZBoundsRasterTexture = GraphBuilder.CreateTexture(ZBoundsRasterTextureDesc, TEXT("WaterQuadTree.ZBoundsRasterTexture"));


	// Raster water body meshes
	{
		TShaderMapRef<FWaterQuadTreeVS> VertexShader(ShaderMap);
		TShaderMapRef<FWaterQuadTreePS> PixelShader(ShaderMap);

		FWaterQuadTreeParameters* PassParameters = GraphBuilder.AllocParameters<FWaterQuadTreeParameters>();
		PassParameters->PS.RenderTargets[0] = FRenderTargetBinding(WaterBodyRasterTexture, ERenderTargetLoadAction::EClear);
		PassParameters->PS.RenderTargets[1] = FRenderTargetBinding(ZBoundsRasterTexture, ERenderTargetLoadAction::EClear);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("WaterQuadTreeRaster"),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, RasterResolution, Draws = MoveTemp(Draws), VertexShader, PixelShader](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, RasterResolution.X, RasterResolution.Y, 1.0f);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<
					CW_RGBA, BO_Max, BF_One, BF_One, BO_Max, BF_One, BF_One,
					CW_RGBA, BO_Max, BF_One, BF_One, BO_Max, BF_One, BF_One>::GetRHI(); // MAX blending
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector3();
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				for (const FDraw& Draw : Draws)
				{
					PassParameters->VS.Transform = Draw.Transform;
					PassParameters->PS.WaterBodyRenderDataIndex = Draw.WaterBodyRenderDataIndex;
					PassParameters->PS.Priority = Draw.Priority;
					PassParameters->PS.WaterBodyMinZ = Draw.MinZ;
					PassParameters->PS.WaterBodyMaxZ = Draw.MaxZ;
					PassParameters->PS.MaxWaveHeight = Draw.MaxWaveHeight;
					PassParameters->PS.bIsRiver = Draw.bIsRiver;

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);
					RHICmdList.SetStreamSource(0, Draw.VertexBuffer, 0);
					RHICmdList.DrawIndexedPrimitive(Draw.IndexBuffer, Draw.BaseVertexIndex, 0, Draw.NumVertices, Draw.FirstIndex, Draw.NumPrimitives, 1);
				}
			});
	}

		// Merge river and non-river water bodies and downsample to quad tree LOD0 resolution
		{
			FWaterQuadTreeMergePS::FPermutationDomain PermutationDomain;
			PermutationDomain.Set<FWaterQuadTreeMergePS::FNumMSAASamples>(NumSamples);
			TShaderMapRef<FWaterQuadTreeMergePS> PixelShader(ShaderMap, PermutationDomain);

			FWaterQuadTreeMergePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterQuadTreeMergePS::FParameters>();
			if (NumSamples > 1)
			{
				PassParameters->WaterBodyRasterTextureMS = WaterBodyRasterTexture;
				PassParameters->ZBoundsRasterTextureMS = ZBoundsRasterTexture;
			}
			else
			{
				PassParameters->WaterBodyRasterTexture = WaterBodyRasterTexture;
				PassParameters->ZBoundsRasterTexture = ZBoundsRasterTexture;
			}
			PassParameters->WaterBodyRenderData = WaterBodyRenderDataBufferSRV;
			PassParameters->SuperSamplingFactor = Params.SuperSamplingFactor;
			PassParameters->RcpCaptureDepthRange = 1.0f / Params.CaptureDepthRange;
			PassParameters->ZBoundsPadding = GWaterQuadTreeZBoundsPadding / Params.CaptureDepthRange;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(QuadTreeTextureRDG, ERenderTargetLoadAction::ENoAction, 0);
			PassParameters->RenderTargets[1] = FRenderTargetBinding(WaterZBoundsTextureRDG, ERenderTargetLoadAction::ENoAction, 0);

			const FIntRect MergeViewport(0, 0, QuadTreeResolution.X, QuadTreeResolution.Y);

			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				ShaderMap,
				RDG_EVENT_NAME("WaterQuadTreeMerge"),
				PixelShader,
				PassParameters,
				MergeViewport);
		}

		// Build the mip chain
		{
			TShaderMapRef<FWaterQuadTreeBuildPS> PixelShader(ShaderMap);

			FIntRect BuildViewport(0, 0, QuadTreeResolution.X, QuadTreeResolution.Y);

			for (int32 MipLevel = 1; MipLevel < NumMipLevels; ++MipLevel)
			{
				FWaterQuadTreeBuildPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterQuadTreeBuildPS::FParameters>();
				PassParameters->QuadTreeTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(QuadTreeTextureRDG, MipLevel - 1));
				PassParameters->WaterZBoundsTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(WaterZBoundsTextureRDG, MipLevel - 1));
				PassParameters->RenderTargets[0] = FRenderTargetBinding(QuadTreeTextureRDG, ERenderTargetLoadAction::ENoAction, MipLevel);
				PassParameters->RenderTargets[1] = FRenderTargetBinding(WaterZBoundsTextureRDG, ERenderTargetLoadAction::ENoAction, MipLevel);

				BuildViewport = BuildViewport / 2;

				FPixelShaderUtils::AddFullscreenPass(
					GraphBuilder,
					ShaderMap,
					RDG_EVENT_NAME("WaterQuadTreeBuild"),
					PixelShader,
					PassParameters,
					BuildViewport);
			}
		}
	
}

void FWaterQuadTreeGPU::Traverse(FRDGBuilder& GraphBuilder, const FTraverseParams& Params) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWaterQuadTreeGPU::Traverse);
	RDG_EVENT_SCOPE(GraphBuilder, "FWaterQuadTreeGPU::Traverse");
	RDG_GPU_STAT_SCOPE(GraphBuilder, FWaterQuadTreeGPU_Traverse);

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	FRDGTexture* QuadTreeTextureRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(QuadTreeTexture, TEXT("WaterQuadTree.QuadTree")));
	FRDGTexture* WaterZBoundsTextureRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(WaterZBoundsTexture, TEXT("WaterQuadTree.WaterSurfaceHeight")));
	FRDGBuffer* WaterBodyRenderDataBufferRDG = GraphBuilder.RegisterExternalBuffer(WaterBodyRenderDataBuffer);
	FRDGBuffer* IndirectArgsBuffer = GraphBuilder.RegisterExternalBuffer(Params.OutIndirectArgsBuffer);
	FRDGBuffer* InstanceDataOffsetsBuffer = GraphBuilder.RegisterExternalBuffer(Params.OutInstanceDataOffsetsBuffer);
	FRDGBuffer* InstanceData0Buffer = GraphBuilder.RegisterExternalBuffer(Params.OutInstanceData0Buffer);
	FRDGBuffer* InstanceData1Buffer = GraphBuilder.RegisterExternalBuffer(Params.OutInstanceData1Buffer);
	FRDGBuffer* InstanceData2Buffer = Params.bWithWaterSelectionSupport ? 
		GraphBuilder.RegisterExternalBuffer(Params.OutInstanceData2Buffer) : 
		GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float) * 4, 1), TEXT("WaterQuadTree.InstanceData2Dummy"));
	
	FRDGBufferUAV* IndirectArgsBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(IndirectArgsBuffer));
	FRDGBufferUAV* InstanceDataOffsetsBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(InstanceDataOffsetsBuffer, PF_R32_UINT));
	FRDGBufferSRV* InstanceDataOffsetsBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InstanceDataOffsetsBuffer, PF_R32_UINT));
	FRDGBufferSRV* WaterBodyRenderDataBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(WaterBodyRenderDataBufferRDG));

	const uint32 NumBucketsPerView = Params.NumDensities * Params.NumMaterials;
	const uint32 NumBucketsTotal = NumBucketsPerView * Params.NumViews;
	const FIntPoint QuadTreeResolution = QuadTreeTextureRDG->Desc.Extent;

	// Initialize indirect args
	{
		TShaderMapRef<FWaterQuadTreeInitializeIndirectArgsCS> ComputeShader(ShaderMap);

		FWaterQuadTreeInitializeIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterQuadTreeInitializeIndirectArgsCS::FParameters>();
		PassParameters->IndirectArgs = IndirectArgsBufferUAV;
		PassParameters->NumDensities = Params.NumDensities;
		PassParameters->NumMaterials = Params.NumMaterials;
		PassParameters->NumViews = Params.NumViews;
		PassParameters->NumQuadsLOD0 = Params.NumQuadsLOD0;
		
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("WaterQuadTreeInitIndirectArgs"), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCount(NumBucketsTotal, 64));
	}

	// Iterate over all views for which to create indirect water draws
	for (uint32 ViewIndex = 0; ViewIndex < Params.NumViews; ++ViewIndex)
	{
		const FPerViewInfo& PerViewInfo = Params.PerViewInfo[ViewIndex];

		const uint32 MaxNumDraws = QuadTreeResolution.X * QuadTreeResolution.Y;
		FRDGBuffer* PackedNodes = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc((1 + MaxNumDraws) * sizeof(uint32)), TEXT("WaterQuadTree.PackedNodes"));
		FRDGBuffer* BucketCounts = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(FMath::Max(1u, NumBucketsPerView) * sizeof(uint32)), TEXT("WaterQuadTree.DrawBucketCounts"));
		FRDGBufferUAV* PackedNodesUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(PackedNodes, PF_R32_UINT));
		FRDGBufferUAV* BucketCountsUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(BucketCounts, PF_R32_UINT));
		FRDGBufferSRV* PackedNodesSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(PackedNodes, PF_R32_UINT));
		FRDGBufferSRV* BucketCountsSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(BucketCounts, PF_R32_UINT));

		// Clear counts and packed nodes counter
		// TODO: only clear the counter at index 0, not the entire buffer
		FComputeShaderUtils::ClearUAV(GraphBuilder, ShaderMap, BucketCountsUAV, 0);
		FComputeShaderUtils::ClearUAV(GraphBuilder, ShaderMap, PackedNodesUAV, 0);

		// Traverse quadtree
		{
			if (Params.DebugShowTile != 0)
			{
				ShaderPrint::SetEnabled(true);
			}

			const bool bEnableShaderPrint = Params.DebugShowTile != 0
				&& PerViewInfo.ViewInfo
				&& ShaderPrint::IsEnabled()
				&& ShaderPrint::IsEnabled(PerViewInfo.ViewInfo->ShaderPrintData)
				&& ShaderPrint::IsValid(PerViewInfo.ViewInfo->ShaderPrintData)
				&& ShaderPrint::IsSupported(GetFeatureLevelShaderPlatform(GMaxRHIFeatureLevel));

			if (bEnableShaderPrint)
			{
				// We'll be potentially drawing a lot of bounding boxes, each made up of 12 line segments, so reserve some space
				ShaderPrint::RequestSpaceForLines(12 * MaxNumDraws);
			}

			FWaterQuadTreeTraverseCS::FPermutationDomain PermutationDomain;
			PermutationDomain.Set<FWaterQuadTreeTraverseCS::FDebugShaderPrint>(bEnableShaderPrint);
			TShaderMapRef<FWaterQuadTreeTraverseCS> ComputeShader(ShaderMap, PermutationDomain);

			const uint32 NumMipLevels = QuadTreeTextureRDG->Desc.NumMips;
			uint32 NumTexelsTotal = 0;
			for (uint32 MipLevelIndex = 0; MipLevelIndex < NumMipLevels; ++MipLevelIndex)
			{
				const FIntPoint MipExtent = FIntPoint(FMath::Max(1, QuadTreeTextureRDG->Desc.Extent.X >> MipLevelIndex), FMath::Max(1, QuadTreeTextureRDG->Desc.Extent.Y >> MipLevelIndex));
				NumTexelsTotal += MipExtent.X * MipExtent.Y;
			}

			FWaterQuadTreeTraverseCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterQuadTreeTraverseCS::FParameters>();
			if (bEnableShaderPrint)
			{
				ShaderPrint::SetParameters(GraphBuilder, PerViewInfo.ViewInfo->ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
			}
			PassParameters->PackedNodes = PackedNodesUAV;
			PassParameters->QuadTreeTexture = QuadTreeTextureRDG;
			PassParameters->WaterZBoundsTexture = WaterZBoundsTextureRDG;
			PassParameters->WaterBodyRenderData = WaterBodyRenderDataBufferSRV;
			PassParameters->HZBTexture = (PerViewInfo.ViewInfo && PerViewInfo.ViewInfo->HZB) ? PerViewInfo.ViewInfo->HZB : GSystemTextures.GetBlackDummy(GraphBuilder);
			PassParameters->HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->TranslatedWorldToClip = PerViewInfo.TranslatedWorldToClip;
			PassParameters->ViewToClip = PerViewInfo.ViewToClip;
			PassParameters->CullingBoundsAABB = FVector4f(Params.CullingBounds.Min.X, Params.CullingBounds.Min.Y, Params.CullingBounds.Max.X, Params.CullingBounds.Max.Y);
			PassParameters->HZBSize = PerViewInfo.ViewInfo ? FVector2f(PerViewInfo.ViewInfo->ViewRect.Size()) : FVector2f::ZeroVector;
			PassParameters->HZBViewSize = PerViewInfo.ViewInfo ? FVector2f(PerViewInfo.ViewInfo->HZBMipmap0Size) : FVector2f::ZeroVector;
			PassParameters->QuadTreePosition = PerViewInfo.QuadTreePositionTranslatedWorldSpace;
			PassParameters->ObserverPosition = PerViewInfo.ObserverPositionTranslatedWorldSpace;
			PassParameters->QuadTreeResolutionX = QuadTreeResolution.X;
			PassParameters->QuadTreeResolutionY = QuadTreeResolution.Y;
			PassParameters->NumDensities = Params.NumDensities;
			PassParameters->NumMaterials = Params.NumMaterials;
			PassParameters->LeafSize = Params.LeafSize;
			PassParameters->LODScale = Params.LODScale;
			PassParameters->CaptureDepthRange = CaptureDepthRange;
			PassParameters->ForceCollapseDensityLevel = -1; // Params.ForceCollapseDensityLevel; // TODO: Properly implement support for this parameter
			PassParameters->NumLODs = NumMipLevels;
			PassParameters->NumDispatchedThreads = NumTexelsTotal;
			PassParameters->DebugShowTile = Params.DebugShowTile;
			PassParameters->bHZBOcclusionCullingEnabled = GWaterQuadTreeOcclusionCulling && (PerViewInfo.ViewInfo && PerViewInfo.ViewInfo->HZB);

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("WaterQuadTreeTraverse(View: %i)", ViewIndex), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCount(NumTexelsTotal, 64));
		}

		// Compute bucket counts
		{
			TShaderMapRef<FWaterQuadTreeBucketCountsCS> ComputeShader(ShaderMap);

			FWaterQuadTreeBucketCountsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterQuadTreeBucketCountsCS::FParameters>();
			PassParameters->BucketCounts = BucketCountsUAV;
			PassParameters->QuadTreeTexture = QuadTreeTextureRDG;
			PassParameters->WaterBodyRenderData = WaterBodyRenderDataBufferSRV;
			PassParameters->PackedNodes = PackedNodesSRV;
			PassParameters->NumDensities = Params.NumDensities;
			PassParameters->NumMaterials = Params.NumMaterials;
			PassParameters->NumDispatchedThreads = FMath::Min(FMath::DivideAndRoundUp(MaxNumDraws, 64u), 65535u) * 64u; // The maximum number of dispatched groups is 64k

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("WaterQuadTreeDrawBucketCounts(View: %i)", ViewIndex), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCount(PassParameters->NumDispatchedThreads, 64));
		}

		// Compute bucket prefix sums
		{
			FWaterQuadTreeBucketPrefixSumCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FWaterQuadTreeBucketPrefixSumCS::FParallelPrefixSum>(GWaterQuadTreeParallelPrefixSum != 0);
			TShaderMapRef<FWaterQuadTreeBucketPrefixSumCS> ComputeShader(ShaderMap, PermutationVector);

			FWaterQuadTreeBucketPrefixSumCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterQuadTreeBucketPrefixSumCS::FParameters>();
			PassParameters->BucketPrefixSums = InstanceDataOffsetsBufferUAV;
			PassParameters->BucketCounts = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(BucketCounts));
			PassParameters->NumBuckets = NumBucketsPerView;
			PassParameters->OutputOffset = ViewIndex * NumBucketsPerView;

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("WaterQuadTreeDrawBucketPrefixSums(View: %i)", ViewIndex), ComputeShader, PassParameters, FIntVector3(1, 1, 1));
		}

		// Generate instance data and fill indirect args
		{
			TShaderMapRef<FWaterQuadTreeGenerateInstanceDataCS> ComputeShader(ShaderMap);

			FWaterQuadTreeGenerateInstanceDataCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWaterQuadTreeGenerateInstanceDataCS::FParameters>();
			PassParameters->IndirectArgs = IndirectArgsBufferUAV;
			PassParameters->InstanceData0 = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(InstanceData0Buffer, PF_A32B32G32R32F));
			PassParameters->InstanceData1 = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(InstanceData1Buffer, PF_A32B32G32R32F));
			PassParameters->InstanceData2 = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(InstanceData2Buffer, PF_A32B32G32R32F));
			PassParameters->QuadTreeTexture = QuadTreeTextureRDG;
			PassParameters->WaterZBoundsTexture = WaterZBoundsTextureRDG;
			PassParameters->WaterBodyRenderData = WaterBodyRenderDataBufferSRV;
			PassParameters->PackedNodes = PackedNodesSRV;
			PassParameters->InstanceDataOffsets = InstanceDataOffsetsBufferSRV;
			PassParameters->QuadTreePosition = PerViewInfo.QuadTreePositionTranslatedWorldSpace;
			PassParameters->ObserverPosition = PerViewInfo.ObserverPositionTranslatedWorldSpace;
			PassParameters->QuadTreeResolutionX = QuadTreeResolution.X;
			PassParameters->QuadTreeResolutionY = QuadTreeResolution.Y;
			PassParameters->NumDensities = Params.NumDensities;
			PassParameters->NumMaterials = Params.NumMaterials;
			PassParameters->NumDispatchedThreads = FMath::Min(FMath::DivideAndRoundUp(MaxNumDraws, 64u), 65535u) * 64u; // The maximum number of dispatched groups is 64k
			PassParameters->BucketIndexOffset = ViewIndex * NumBucketsPerView;
			PassParameters->NumLODs = QuadTreeTextureRDG->Desc.NumMips;
			PassParameters->LeafSize = Params.LeafSize;
			PassParameters->LODScale = Params.LODScale;
			PassParameters->CaptureDepthRange = CaptureDepthRange;
			PassParameters->bWithWaterSelectionSupport = Params.bWithWaterSelectionSupport;
			PassParameters->bLODMorphingEnabled = Params.bLODMorphingEnabled;

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("WaterQuadTreeGenerateDraws(View: %i)", ViewIndex), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCount(PassParameters->NumDispatchedThreads, 64));
		}
	}
}
