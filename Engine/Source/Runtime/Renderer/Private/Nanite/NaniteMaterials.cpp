// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteMaterials.h"
#include "Async/ParallelFor.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "NaniteDrawList.h"
#include "NaniteSceneProxy.h"
#include "NaniteVertexFactory.h"
#include "NaniteVisualizationData.h"
#include "NaniteRayTracing.h"
#include "NaniteComposition.h"
#include "NaniteShading.h"
#include "Rendering/NaniteResources.h"
#include "Rendering/NaniteStreamingManager.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "ScreenPass.h"
#include "GPUScene.h"
#include "ClearQuad.h"
#include "RendererModule.h"
#include "PixelShaderUtils.h"
#include "Lumen/LumenSceneCardCapture.h"
#include "Substrate/Substrate.h"
#include "SystemTextures.h"
#include "BasePassRendering.h"
#include "VariableRateShadingImageManager.h"
#include "Lumen/Lumen.h"
#include "ComponentRecreateRenderStateContext.h"
#include "InstanceDataSceneProxy.h"

extern TAutoConsoleVariable<int32> CVarParallelBasePassBuild;

static TAutoConsoleVariable<int32> CVarNaniteMultipleSceneViewsInOnePass(
	TEXT("r.Nanite.MultipleSceneViewsInOnePass"),
	1,
	TEXT("Supports rendering multiple views (FSceneView) whenever possible. Currently only ISR stereo rendering is supported."),
	ECVF_RenderThreadSafe
	);

extern int32 GNaniteShowStats;

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteMaterialPassParameters, )
	RDG_BUFFER_ACCESS(ShadingBinArgs, ERHIAccess::IndirectArgs)

	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)	// To access VTFeedbackBuffer
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRasterUniformParameters, NaniteRaster)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteShadingUniformParameters, NaniteShading)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, BasePass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardPassUniformParameters, CardPass)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget0)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget1)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget2)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget3)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget4)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget5)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget6)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget7)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, OutTargets)

	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, RecordArgBuffer)
END_SHADER_PARAMETER_STRUCT()

TRDGUniformBufferRef<FNaniteShadingUniformParameters> CreateDebugNaniteShadingUniformBuffer(FRDGBuilder& GraphBuilder)
{
	FNaniteShadingUniformParameters* UniformParameters = GraphBuilder.AllocParameters<FNaniteShadingUniformParameters>();

	UniformParameters->ClusterPageData				= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	UniformParameters->HierarchyBuffer				= Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
	UniformParameters->VisibleClustersSWHW			= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));

#if RHI_RAYTRACING
	UniformParameters->RayTracingCutError			= Nanite::GRayTracingManager.GetCutError();
	UniformParameters->RayTracingDataBuffer			= Nanite::GRayTracingManager.GetAuxiliaryDataSRV(GraphBuilder);
#else
	UniformParameters->RayTracingCutError			= 0.0f;
	UniformParameters->RayTracingDataBuffer			= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));
#endif

	UniformParameters->ShadingBinData				= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder), PF_R32_UINT);

	const FRDGSystemTextures& SystemTextures		= FRDGSystemTextures::Get(GraphBuilder);
	UniformParameters->VisBuffer64					= SystemTextures.Black;
	UniformParameters->DbgBuffer64					= SystemTextures.Black;
	UniformParameters->DbgBuffer32					= SystemTextures.Black;
	UniformParameters->ShadingMask					= SystemTextures.Black;
	UniformParameters->MultiViewIndices				= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));
	UniformParameters->MultiViewRectScaleOffsets	= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FVector4>(GraphBuilder));
	UniformParameters->InViews						= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FPackedNaniteView>(GraphBuilder));

	return GraphBuilder.CreateUniformBuffer(UniformParameters);
}

TRDGUniformBufferRef<FNaniteRasterUniformParameters> CreateDebugNaniteRasterUniformBuffer(FRDGBuilder& GraphBuilder, uint32 InstanceSceneDataSOAStride)
{
	FNaniteRasterUniformParameters* UniformParameters	= GraphBuilder.AllocParameters<FNaniteRasterUniformParameters>();
	
	UniformParameters->PageConstants.X					= InstanceSceneDataSOAStride;
	UniformParameters->PageConstants.Y					= Nanite::GStreamingManager.GetMaxStreamingPages();
	UniformParameters->MaxNodes							= Nanite::FGlobalResources::GetMaxNodes();
	UniformParameters->MaxVisibleClusters				= Nanite::FGlobalResources::GetMaxVisibleClusters();
	UniformParameters->MaxPatchesPerGroup				= 0u;
	UniformParameters->MeshPass							= 0u;
	UniformParameters->InvDiceRate						= 1.0f;
	UniformParameters->RenderFlags						= 0u;
	UniformParameters->DebugFlags						= 0u;
	
	return GraphBuilder.CreateUniformBuffer(UniformParameters);
}