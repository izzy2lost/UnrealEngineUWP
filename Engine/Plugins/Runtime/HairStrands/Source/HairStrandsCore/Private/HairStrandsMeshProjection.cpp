// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsMeshProjection.h"
#include "MeshMaterialShader.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "CommonRenderResources.h"
#include "RHIStaticStates.h"
#include "SkeletalRenderPublic.h"
#include "RayTracingDynamicGeometryCollection.h"
#include "RenderGraphUtils.h"
#include "GroomResources.h"
#include "SystemTextures.h"

static int32 GHairProjectionMaxTrianglePerProjectionIteration = 8;
static FAutoConsoleVariableRef CVarHairProjectionMaxTrianglePerProjectionIteration(TEXT("r.HairStrands.Projection.MaxTrianglePerIteration"), GHairProjectionMaxTrianglePerProjectionIteration, TEXT("Change the number of triangles which are iterated over during one projection iteration step. In kilo triangle (e.g., 8 == 8000 triangles). Default is 8."));

static int32 GHairStrandsUseGPUPositionOffset = 1;
static FAutoConsoleVariableRef CVarHairStrandsUseGPUPositionOffset(TEXT("r.HairStrands.UseGPUPositionOffset"), GHairStrandsUseGPUPositionOffset, TEXT("Use GPU position offset to improve hair strands position precision."));

///////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_HAIRSTRANDS_SECTION_COUNT 255
#define MAX_HAIRSTRANDS_SECTION_BITOFFSET 24

uint32 GetHairStrandsMaxSectionCount()
{
	return MAX_HAIRSTRANDS_SECTION_COUNT;
}

uint32 GetHairStrandsMaxTriangleCount()
{
	return (1 << MAX_HAIRSTRANDS_SECTION_BITOFFSET) - 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

FHairStrandsProjectionMeshData ExtractMeshData(FSkeletalMeshRenderData* RenderData)
{
	FHairStrandsProjectionMeshData MeshData;
	uint32 LODIndex = 0;
	for (FSkeletalMeshLODRenderData& LODRenderData : RenderData->LODRenderData)
	{
		FHairStrandsProjectionMeshData::LOD& LOD = MeshData.LODs.AddDefaulted_GetRef();
		uint32 SectionIndex = 0;
		for (FSkelMeshRenderSection& InSection : LODRenderData.RenderSections)
		{
			// Pick between float and halt
			const uint32 UVSizeInByte = (LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs() ? 4 : 2) * 2;

			FHairStrandsProjectionMeshData::Section& OutSection = LOD.Sections.AddDefaulted_GetRef();
			OutSection.UVsChannelOffset = 0; // Assume that we needs to pair meshes based on UVs 0
			OutSection.UVsChannelCount = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
			OutSection.UVsBuffer = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordsSRV();
			OutSection.PositionBuffer = LODRenderData.StaticVertexBuffers.PositionVertexBuffer.GetSRV();
			OutSection.IndexBuffer = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->GetSRV();
			OutSection.TotalVertexCount = LODRenderData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
			OutSection.TotalIndexCount = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->Num();
			OutSection.NumPrimitives = InSection.NumTriangles;
			OutSection.NumVertices = InSection.NumVertices;
			OutSection.VertexBaseIndex = InSection.BaseVertexIndex;
			OutSection.IndexBaseIndex = InSection.BaseIndex;
			OutSection.SectionIndex = SectionIndex;
			OutSection.LODIndex = LODIndex;

			++SectionIndex;
		}
		++LODIndex;
	}

	return MeshData;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
class FSkinUpdateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSkinUpdateCS);
	SHADER_USE_PARAMETER_STRUCT(FSkinUpdateCS, FGlobalShader);

	
	class FPrevious : SHADER_PERMUTATION_BOOL("PERMUTATION_PREV");
	class FUnlimitedBoneInfluence : SHADER_PERMUTATION_BOOL("GPUSKIN_UNLIMITED_BONE_INFLUENCE");
	class FUseExtraInfluence : SHADER_PERMUTATION_BOOL("GPUSKIN_USE_EXTRA_INFLUENCES");
	class FBoneIndexUint16 : SHADER_PERMUTATION_BOOL("GPUSKIN_BONE_INDEX_UINT16");
	class FBoneWeightUint16 : SHADER_PERMUTATION_BOOL("GPUSKIN_BONE_WEIGHTS_UINT16");	
	using FPermutationDomain = TShaderPermutationDomain<FUnlimitedBoneInfluence, FUseExtraInfluence, FBoneIndexUint16, FBoneWeightUint16, FPrevious>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumVertexToProcess)
		SHADER_PARAMETER(uint32, NumTotalVertices)
		SHADER_PARAMETER(uint32, SectionVertexBaseIndex)
		SHADER_PARAMETER(uint32, WeightIndexSize)
		SHADER_PARAMETER(uint32, WeightStride)
		SHADER_PARAMETER(uint32, BonesOffset)
		SHADER_PARAMETER_SRV(Buffer<uint>, WeightLookup)
		SHADER_PARAMETER_SRV(Buffer<float4>, BoneMatrices)
		SHADER_PARAMETER_SRV(Buffer<float4>, PrevBoneMatrices)
		SHADER_PARAMETER_SRV(Buffer<uint>, VertexWeights)
		SHADER_PARAMETER_SRV(Buffer<float>, RestPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, DeformedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PrevDeformedPositions)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
};

IMPLEMENT_GLOBAL_SHADER(FSkinUpdateCS, "/Engine/Private/HairStrands/HairStrandsSkinUpdate.usf", "UpdateSkinPositionCS", SF_Compute);

void AddSkinUpdatePass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	uint32 SectionIndex,
	uint32 BonesOffset,	
	FSkeletalMeshLODRenderData& RenderData,
	FRHIShaderResourceView* BoneMatrices,
	FRHIShaderResourceView* PrevBoneMatrices,
	FRDGBufferRef OutDeformedPosition,
	FRDGBufferRef OutPrevDeformedPosition)
{
	check(BoneMatrices);

	FSkinWeightVertexBuffer* SkinWeight = &RenderData.SkinWeightVertexBuffer;
	const FSkelMeshRenderSection& Section = RenderData.RenderSections[SectionIndex];
	const uint32 NumVertexToProcess = Section.NumVertices;
	const uint32 SectionVertexBaseIndex = Section.BaseVertexIndex;
	const uint32 NumTotalVertices = RenderData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();

	const bool bPrevPosition = OutPrevDeformedPosition != nullptr && PrevBoneMatrices != nullptr;
	
	FSkinUpdateCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSkinUpdateCS::FParameters>();
	Parameters->WeightIndexSize = SkinWeight->GetBoneIndexByteSize() | (SkinWeight->GetBoneWeightByteSize() << 8);
	Parameters->NumVertexToProcess = NumVertexToProcess;
	Parameters->NumTotalVertices = NumTotalVertices;
	Parameters->SectionVertexBaseIndex = SectionVertexBaseIndex;
	Parameters->WeightStride = SkinWeight->GetConstantInfluencesVertexStride();
	Parameters->WeightLookup = SkinWeight->GetLookupVertexBuffer()->GetSRV();
	Parameters->BonesOffset = BonesOffset;
	Parameters->BoneMatrices = BoneMatrices;
	Parameters->VertexWeights = SkinWeight->GetDataVertexBuffer()->GetSRV();
	Parameters->RestPositions = RenderData.StaticVertexBuffers.PositionVertexBuffer.GetSRV();
	Parameters->DeformedPositions = GraphBuilder.CreateUAV(OutDeformedPosition, PF_R32_FLOAT);
	if (bPrevPosition)
	{
		check(PrevBoneMatrices);
		Parameters->PrevBoneMatrices = BoneMatrices;
		Parameters->PrevDeformedPositions = GraphBuilder.CreateUAV(OutPrevDeformedPosition, PF_R32_FLOAT);
	}

	FSkinUpdateCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FSkinUpdateCS::FUnlimitedBoneInfluence>(SkinWeight->GetBoneInfluenceType() == GPUSkinBoneInfluenceType::UnlimitedBoneInfluence);
	PermutationVector.Set<FSkinUpdateCS::FUseExtraInfluence>(SkinWeight->GetMaxBoneInfluences() > MAX_INFLUENCES_PER_STREAM);
	PermutationVector.Set<FSkinUpdateCS::FBoneIndexUint16 >(SkinWeight->Use16BitBoneIndex());
	PermutationVector.Set<FSkinUpdateCS::FBoneIndexUint16 >(SkinWeight->Use16BitBoneWeight());
	PermutationVector.Set<FSkinUpdateCS::FPrevious>(bPrevPosition);

	const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(NumVertexToProcess, 64);
	check(DispatchGroupCount.X <= GRHIMaxDispatchThreadGroupsPerDimension.X);
	TShaderMapRef<FSkinUpdateCS> ComputeShader(ShaderMap, PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::UpdateSkinPosition(%s,Section:%d)", bPrevPosition ? TEXT("Curr,Prev") : TEXT("Curr"), SectionIndex),
		ComputeShader,
		Parameters,
		DispatchGroupCount);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairUpdateMeshTriangleCS : public FGlobalShader
{
private:
	DECLARE_GLOBAL_SHADER(FHairUpdateMeshTriangleCS);
	SHADER_USE_PARAMETER_STRUCT(FHairUpdateMeshTriangleCS, FGlobalShader);

	class FPositionType : SHADER_PERMUTATION_BOOL("PERMUTATION_POSITION_TYPE");
	class FPrevious : SHADER_PERMUTATION_BOOL("PERMUTATION_PREVIOUS"); 
	using FPermutationDomain = TShaderPermutationDomain<FPositionType, FPrevious>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxSectionCount)
		SHADER_PARAMETER(uint32, MaxUniqueTriangleCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, MeshSectionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer)
		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer)
		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer)
		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer)
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, UniqueTriangleIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutUniqueTrianglePrevPosition)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutUniqueTriangleCurrPosition)
	END_SHADER_PARAMETER_STRUCT()

public:
	static uint32 GetGroupSize() { return 128; }
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_MESH_UPDATE"), 1);
		OutEnvironment.SetDefine(TEXT("GROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairUpdateMeshTriangleCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainCS", SF_Compute);

bool AddHairStrandUpdateMeshTrianglesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const uint32 UniqueTriangleCount, 
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FRDGBufferSRVRef UniqueTriangleIndexSRV,
	FRDGBufferUAVRef OutputCurrUAV,
	FRDGBufferUAVRef OutputPrevUAV)
{
	const uint32 SectionCount = MeshData.Sections.Num();

	// If a skel. mesh is not streaming yet, its SRV will be null
	const bool bValid = SectionCount > 0 && SectionCount < GetHairStrandsMaxSectionCount();
	const bool bReady = MeshData.Sections[0].IndexBuffer != nullptr && MeshData.Sections[0].UVsBuffer != nullptr && (MeshData.Sections[0].RDGPositionBuffer != nullptr || MeshData.Sections[0].PositionBuffer != nullptr);
	if (!bReady || !bValid)
	{
		return false;
	}

	FHairUpdateMeshTriangleCS::FParameters CommonParameters;
	CommonParameters.MaxUniqueTriangleCount 		= UniqueTriangleCount;
	CommonParameters.MaxSectionCount 				= SectionCount;
	CommonParameters.RDGMeshPositionBuffer			= MeshData.Sections[0].RDGPositionBuffer;
	CommonParameters.RDGMeshPreviousPositionBuffer 	= MeshData.Sections[0].RDGPreviousPositionBuffer;
	CommonParameters.MeshPositionBuffer				= MeshData.Sections[0].PositionBuffer;
	CommonParameters.MeshPreviousPositionBuffer		= MeshData.Sections[0].PreviousPositionBuffer;
	CommonParameters.MeshIndexBuffer				= MeshData.Sections[0].IndexBuffer;
	CommonParameters.MeshUVsBuffer					= MeshData.Sections[0].UVsBuffer;
	CommonParameters.UniqueTriangleIndices 			= UniqueTriangleIndexSRV;
	CommonParameters.OutUniqueTriangleCurrPosition	= OutputCurrUAV;
	CommonParameters.OutUniqueTrianglePrevPosition	= OutputPrevUAV;

	struct FSectionData
	{
		uint32 TotalIndexCount;
		uint32 TotalVertexCount;
		uint32 IndexBaseIndex;
		uint32 UVsChannelOffset : 8;
		uint32 UVsChannelCount : 8;
		uint32 bIsSwapped : 8;
		uint32 Pad : 8;
	};
	TArray<FSectionData> SectionDatas;
	SectionDatas.SetNum(SectionCount);
	for (uint32 SectionIt = 0; SectionIt < SectionCount; ++SectionIt)
	{
		const FHairStrandsProjectionMeshData::Section& MeshSectionData = MeshData.Sections[SectionIt];
		SectionDatas[SectionIt].TotalIndexCount	= MeshSectionData.TotalIndexCount;
		SectionDatas[SectionIt].TotalVertexCount= MeshSectionData.TotalVertexCount;
		SectionDatas[SectionIt].IndexBaseIndex	= MeshSectionData.IndexBaseIndex;
		SectionDatas[SectionIt].UVsChannelOffset= MeshSectionData.UVsChannelOffset;
		SectionDatas[SectionIt].UVsChannelCount	= MeshSectionData.UVsChannelCount;
		SectionDatas[SectionIt].bIsSwapped		= MeshData.Sections[SectionIt].PositionBuffer != CommonParameters.MeshPositionBuffer ? 1u : 0u;
		SectionDatas[SectionIt].Pad				= 0u;

		// Sanity check
		check(MeshSectionData.SectionIndex == SectionIt);
		check(MeshSectionData.UVsChannelOffset < 255);
		check(MeshSectionData.UVsChannelCount < 255);
#if 1 // Relaxed check, with optional buffer swap
		check(CommonParameters.RDGMeshPositionBuffer == MeshData.Sections[SectionIt].RDGPositionBuffer || CommonParameters.RDGMeshPreviousPositionBuffer == MeshData.Sections[SectionIt].RDGPreviousPositionBuffer);
		check(CommonParameters.MeshPositionBuffer    == MeshData.Sections[SectionIt].PositionBuffer    || CommonParameters.MeshPositionBuffer    == MeshData.Sections[SectionIt].PreviousPositionBuffer);
#else
		check(CommonParameters.RDGMeshPositionBuffer		== MeshData.Sections[SectionIt].RDGPositionBuffer);
		check(CommonParameters.RDGMeshPreviousPositionBuffer== MeshData.Sections[SectionIt].RDGPreviousPositionBuffer);
		check(CommonParameters.MeshPositionBuffer			== MeshData.Sections[SectionIt].PositionBuffer);
		check(CommonParameters.MeshPreviousPositionBuffer	== MeshData.Sections[SectionIt].PreviousPositionBuffer);
#endif
		check(CommonParameters.MeshIndexBuffer				== MeshData.Sections[SectionIt].IndexBuffer);
		check(CommonParameters.MeshUVsBuffer				== MeshData.Sections[SectionIt].UVsBuffer);
	}

	// If no previous position buffer available, reusing the current position buffers
	if (CommonParameters.MeshPreviousPositionBuffer == nullptr)
	{
		CommonParameters.MeshPreviousPositionBuffer = CommonParameters.MeshPositionBuffer;
	}
	if (CommonParameters.RDGMeshPreviousPositionBuffer == nullptr)
	{
		CommonParameters.RDGMeshPreviousPositionBuffer = CommonParameters.RDGMeshPositionBuffer;
	}

	FRDGBufferRef SectionBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("Hair.SkelMeshSectionBuffer"), sizeof(FSectionData),  SectionDatas.Num(), SectionDatas.GetData(), sizeof(FSectionData) * SectionDatas.Num());
	CommonParameters.MeshSectionBuffer = GraphBuilder.CreateSRV(SectionBuffer);

	const bool bUseRDGPositionBuffer = CommonParameters.RDGMeshPositionBuffer != nullptr;
	const bool bComputePreviousDeformedPosition = OutputPrevUAV != nullptr;
	{
		FHairUpdateMeshTriangleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHairUpdateMeshTriangleCS::FParameters>();
		*PassParameters = CommonParameters;

		FHairUpdateMeshTriangleCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FHairUpdateMeshTriangleCS::FPrevious>(bComputePreviousDeformedPosition);
		PermutationVector.Set<FHairUpdateMeshTriangleCS::FPositionType>(bUseRDGPositionBuffer ? 1 : 0);

		const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(UniqueTriangleCount, FHairUpdateMeshTriangleCS::GetGroupSize());
		check(DispatchGroupCount.X <= GRHIMaxDispatchThreadGroupsPerDimension.X);
		TShaderMapRef<FHairUpdateMeshTriangleCS> ComputeShader(ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HairStrands::TriangleMeshUpdate(%s,%s)", bComputePreviousDeformedPosition ? TEXT("Previous/Current, Current") : TEXT("Current"), bUseRDGPositionBuffer ? TEXT("RDGBuffer") : TEXT("SkinCacheBuffer")),
			ComputeShader,
			PassParameters,
			DispatchGroupCount);
	}

	return true;
}

void AddHairStrandUpdateMeshTrianglesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 MeshLODIndex,
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources)
{	
	if (RestResources->GetRootCount() == 0 || !RestResources->LODs.IsValidIndex(MeshLODIndex))
	{
		return;
	}

	FHairStrandsRestRootResource::FLOD& RestLODData = RestResources->LODs[MeshLODIndex];
	check(RestLODData.LODIndex == MeshLODIndex);
	check(RestResources->BulkData.Header.LODs.IsValidIndex(MeshLODIndex));
	const uint32 UniqueTriangleCount = RestResources->BulkData.Header.LODs[MeshLODIndex].UniqueTriangleCount;
	if (UniqueTriangleCount == 0)
	{
		return;
	}

	FRDGImportedBuffer OutputCurrBuffer = Register(GraphBuilder, DeformedResources->LODs[MeshLODIndex].GetDeformedUniqueTrianglePositionBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateUAV, ERDGUnorderedAccessViewFlags::SkipBarrier);
	FRDGImportedBuffer OutputPrevBuffer;
	const bool bComputePreviousDeformedPosition = IsHairStrandContinuousDecimationReorderingEnabled();
	if (bComputePreviousDeformedPosition)
	{
		Register(GraphBuilder, DeformedResources->LODs[MeshLODIndex].GetDeformedUniqueTrianglePositionBuffer(FHairStrandsDeformedRootResource::FLOD::Previous), ERDGImportedBufferFlags::CreateUAV, ERDGUnorderedAccessViewFlags::SkipBarrier);
	}

	if (AddHairStrandUpdateMeshTrianglesPass(
		GraphBuilder,
		ShaderMap,
		UniqueTriangleCount,
		MeshData,
		RegisterAsSRV(GraphBuilder, RestLODData.UniqueTriangleIndexBuffer),
		OutputCurrBuffer.UAV,
		OutputPrevBuffer.UAV))
	{

		GraphBuilder.SetBufferAccessFinal(OutputCurrBuffer.Buffer, ERHIAccess::SRVMask);
		if (OutputPrevBuffer.Buffer)
		{
			GraphBuilder.SetBufferAccessFinal(OutputPrevBuffer.Buffer, ERHIAccess::SRVMask);
		}
	
		// Update the last known mesh LOD for which the root resources has been updated
		DeformedResources->MeshLODIndex = MeshLODIndex;
		DeformedResources->LODs[MeshLODIndex].Status = FHairStrandsDeformedRootResource::FLOD::EStatus::Completed;	
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairMeshesInterpolateCS : public FGlobalShader
{
private:
	DECLARE_GLOBAL_SHADER(FHairMeshesInterpolateCS);
	SHADER_USE_PARAMETER_STRUCT(FHairMeshesInterpolateCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VertexCount)
		SHADER_PARAMETER(uint32, MaxSampleCount)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, RestSamplePositionsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, MeshSampleWeightsBuffer)

		SHADER_PARAMETER_SRV(Buffer, RestPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutDeformedPositionBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_HAIRMESHES"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairMeshesInterpolateCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainHairMeshesCS", SF_Compute);

template<typename TRestResource, typename TDeformedResource>
void InternalAddHairRBFInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const bool bCards,
	const int32 MeshLODIndex,
	TRestResource* RestResources,
	TDeformedResource* DeformedResources,
	FHairStrandsRestRootResource* RestRootResources,
	FHairStrandsDeformedRootResource* DeformedRootResources)
{
	const uint32 VertexCount = RestResources ? RestResources->GetVertexCount() : 0;
	if (!RestResources || !DeformedResources || !RestRootResources || !DeformedRootResources || VertexCount == 0 || MeshLODIndex >= RestRootResources->LODs.Num() || MeshLODIndex < 0)
	{
		return;
	}

	// Copy current to previous position before update the current position. This allows to have correct motion vectors.
	FRDGImportedBuffer DeformedPositionBuffer_Curr = Register(GraphBuilder, DeformedResources->GetBuffer(TDeformedResource::EFrameType::Current), ERDGImportedBufferFlags::CreateUAV);
	FRDGImportedBuffer DeformedPositionBuffer_Prev = Register(GraphBuilder, DeformedResources->GetBuffer(TDeformedResource::EFrameType::Previous), ERDGImportedBufferFlags::CreateUAV);
	AddCopyBufferPass(GraphBuilder, DeformedPositionBuffer_Prev.Buffer, DeformedPositionBuffer_Curr.Buffer);

	FHairStrandsRestRootResource::FLOD& RestLODData = RestRootResources->LODs[MeshLODIndex];
	FHairStrandsDeformedRootResource::FLOD& DeformedLODData = DeformedRootResources->LODs[MeshLODIndex];
	FHairMeshesInterpolateCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairMeshesInterpolateCS::FParameters>();
	Parameters->VertexCount = VertexCount;
	Parameters->MaxSampleCount = RestLODData.SampleCount;

	Parameters->RestPositionBuffer			= RestResources->RestPositionBuffer.ShaderResourceViewRHI;
	Parameters->OutDeformedPositionBuffer	= DeformedPositionBuffer_Curr.UAV;

	Parameters->RestSamplePositionsBuffer	= RegisterAsSRV(GraphBuilder, RestLODData.RestSamplePositionsBuffer);
	Parameters->MeshSampleWeightsBuffer		= RegisterAsSRV(GraphBuilder, DeformedLODData.GetMeshSampleWeightsBuffer(FHairStrandsDeformedRootResource::FLOD::Current));

	const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(VertexCount, 128);
	check(DispatchGroupCount.X <= GRHIMaxDispatchThreadGroupsPerDimension.X);
	TShaderMapRef<FHairMeshesInterpolateCS> ComputeShader(ShaderMap);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::HairInterpolationRBF(%s)", bCards ? TEXT("Cards") : TEXT("Meshes")),
		ComputeShader,
		Parameters,
		DispatchGroupCount);

	GraphBuilder.SetBufferAccessFinal(DeformedPositionBuffer_Curr.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(DeformedPositionBuffer_Prev.Buffer, ERHIAccess::SRVMask);
	
}

void AddHairMeshesRBFInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 MeshLODIndex,
	FHairMeshesRestResource* RestResources,
	FHairMeshesDeformedResource* DeformedResources,
	FHairStrandsRestRootResource* RestRootResources,
	FHairStrandsDeformedRootResource* DeformedRootResources)
{
	InternalAddHairRBFInterpolationPass(
		GraphBuilder,
		ShaderMap,
		false,
		MeshLODIndex,
		RestResources,
		DeformedResources,
		RestRootResources,
		DeformedRootResources);
}

void AddHairCardsRBFInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 MeshLODIndex,
	FHairCardsRestResource* RestResources,
	FHairCardsDeformedResource* DeformedResources,
	FHairStrandsRestRootResource* RestRootResources,
	FHairStrandsDeformedRootResource* DeformedRootResources)
{
	InternalAddHairRBFInterpolationPass(
		GraphBuilder,
		ShaderMap,
		true,
		MeshLODIndex,
		RestResources,
		DeformedResources,
		RestRootResources,
		DeformedRootResources);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
class FHairInitMeshSamplesCS : public FGlobalShader
{
public:
	const static uint32 SectionArrayCount = 16; // This defines the number of sections managed for each iteration pass
private:
	DECLARE_GLOBAL_SHADER(FHairInitMeshSamplesCS);
	SHADER_USE_PARAMETER_STRUCT(FHairInitMeshSamplesCS, FGlobalShader);

	class FPositionType : SHADER_PERMUTATION_INT("PERMUTATION_POSITION_TYPE", 2);
	using FPermutationDomain = TShaderPermutationDomain<FPositionType>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxSampleCount)
		SHADER_PARAMETER(uint32, MaxVertexCount)
		SHADER_PARAMETER(uint32, PassSectionCount)

		SHADER_PARAMETER_ARRAY(FUintVector4, PackedSectionScalars, [SectionArrayCount])

		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer0)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer1)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer2)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer3)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer4)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer5)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer6)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer7)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer0)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer1)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer2)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer3)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer4)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer5)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer6)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer7)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, SampleIndicesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutSamplePositionsBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MAX_SECTION_COUNT"), SectionArrayCount);
		OutEnvironment.SetDefine(TEXT("SHADER_SAMPLE_INIT"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairInitMeshSamplesCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainCS", SF_Compute);

// Manual packing for mesh section scalars.  MUST MATCH WITH HairStrandsMesh.usf
// PackedSectionScalars: SectionVertexOffset, SectionVertexCount, SectionBufferIndex, *free*
inline void SetSectionVertexOffset(FHairInitMeshSamplesCS::FParameters& Params, uint32 SectionIndex, uint32 SectionVertexOffset) { Params.PackedSectionScalars[SectionIndex].X = SectionVertexOffset; }
inline void SetSectionVertexCount(FHairInitMeshSamplesCS::FParameters& Params, uint32 SectionIndex, uint32 SectionVertexCount)   { Params.PackedSectionScalars[SectionIndex].Y = SectionVertexCount; }
inline void SetSectionBufferIndex(FHairInitMeshSamplesCS::FParameters& Params, uint32 SectionIndex, uint32 SectionBufferIndex)   { Params.PackedSectionScalars[SectionIndex].Z = SectionBufferIndex; }



void AddHairStrandInitMeshSamplesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources)
{
	if (LODIndex < 0)
	{
		return;
	}

	if ((LODIndex >= RestResources->LODs.Num() || LODIndex >= DeformedResources->LODs.Num()))
	{
		return;
	}

	FHairStrandsRestRootResource::FLOD& RestLODData = RestResources->LODs[LODIndex];
	check(RestLODData.LODIndex == LODIndex);

	const uint32 SectionCount = MeshData.Sections.Num();
	const uint32 MaxSupportedSectionCount = GetHairStrandsMaxSectionCount();
	check(SectionCount < MaxSupportedSectionCount);
	if (SectionCount == 0 || SectionCount >= MaxSupportedSectionCount)
	{
		return;
	}

	// When the number of section of a mesh is above FHairUpdateMeshTriangleCS::SectionArrayCount, the update is split into several passes
	const TArray<uint32>& ValidSectionIndices = RestResources->BulkData.GetValidSectionIndices(LODIndex);
	const uint32 ValidSectionCount = ValidSectionIndices.Num();
	const uint32 PassCount = FMath::DivideAndRoundUp(ValidSectionCount, FHairInitMeshSamplesCS::SectionArrayCount);

	if (SectionCount > 0 && RestLODData.SampleCount > 0)
	{
		FRDGImportedBuffer OutBuffer;
		{
			FHairStrandsDeformedRootResource::FLOD& DeformedLODData = DeformedResources->LODs[LODIndex];
			check(DeformedLODData.LODIndex == LODIndex);

			OutBuffer = Register(GraphBuilder, DeformedLODData.GetDeformedSamplePositionsBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateUAV);
		}
		for (uint32 PassIt = 0; PassIt < PassCount; ++PassIt)
		{
			FHairInitMeshSamplesCS::FParameters Parameters;

			for (uint32 DataIndex = 0; DataIndex < FHairInitMeshSamplesCS::SectionArrayCount; ++DataIndex)
			{
				SetSectionBufferIndex(Parameters, DataIndex, 0);
				SetSectionVertexOffset(Parameters, DataIndex, 0);
				SetSectionVertexCount(Parameters, DataIndex, 0);
			}

			const int32 PassSectionStart = PassIt * FHairInitMeshSamplesCS::SectionArrayCount;
			const int32 PassSectionCount = FMath::Min(ValidSectionCount - PassSectionStart, FHairInitMeshSamplesCS::SectionArrayCount);
			Parameters.PassSectionCount = PassSectionCount;

			struct FMeshSectionBuffers
			{
				uint32 SectionBufferIndex = 0;
				FRDGBufferSRVRef RDGPositionBuffer = nullptr;
				FRDGBufferSRVRef RDGPreviousPositionBuffer = nullptr;
				FRHIShaderResourceView* PositionBuffer = nullptr;
				FRHIShaderResourceView* PreviousPositionBuffer = nullptr;
			};
			TMap<FRHIShaderResourceView*, FMeshSectionBuffers>	UniqueMeshSectionBuffers;
			TMap<FRDGBufferSRVRef, FMeshSectionBuffers>		UniqueMeshSectionBuffersRDG;

			#define SETPARAMETERS(OutParameters, InMeshSectionData, Index) \
				OutParameters.RDGVertexPositionsBuffer##Index = InMeshSectionData.RDGPositionBuffer; \
				OutParameters.VertexPositionsBuffer##Index = InMeshSectionData.PositionBuffer;

			auto SetMeshSectionBuffers = [&Parameters](uint32 UniqueIndex, const FHairStrandsProjectionMeshData::Section& MeshSectionData)
			{
				switch (UniqueIndex)
				{
				case 0: SETPARAMETERS(Parameters, MeshSectionData, 0); break;
				case 1: SETPARAMETERS(Parameters, MeshSectionData, 1); break;
				case 2: SETPARAMETERS(Parameters, MeshSectionData, 2); break;
				case 3: SETPARAMETERS(Parameters, MeshSectionData, 3); break;
				case 4: SETPARAMETERS(Parameters, MeshSectionData, 4); break;
				case 5: SETPARAMETERS(Parameters, MeshSectionData, 5); break;
				case 6: SETPARAMETERS(Parameters, MeshSectionData, 6); break;
				case 7: SETPARAMETERS(Parameters, MeshSectionData, 7); break;
				}
			};

			#undef SETPARAMETERS

			uint32 UniqueMeshSectionBufferIndex = 0;
			bool bUseRDGPositionBuffer = false;
			for (int32 SectionStartIt = PassSectionStart, SectionItEnd = PassSectionStart + PassSectionCount; SectionStartIt < SectionItEnd; ++SectionStartIt)
			{
				const int32 SectionIt = SectionStartIt - PassSectionStart;
				const int32 SectionIndex = ValidSectionIndices[SectionStartIt];

				const FHairStrandsProjectionMeshData::Section& MeshSectionData = MeshData.Sections[SectionIndex];

				SetSectionVertexOffset(Parameters, SectionIt, MeshSectionData.VertexBaseIndex);
				SetSectionVertexCount(Parameters, SectionIt, MeshSectionData.NumVertices);

				const FMeshSectionBuffers* Buffers = nullptr;
				if (MeshSectionData.PositionBuffer)
				{
					Buffers = UniqueMeshSectionBuffers.Find(MeshSectionData.PositionBuffer);
				}
				else if (MeshSectionData.RDGPositionBuffer)
				{
					Buffers = UniqueMeshSectionBuffersRDG.Find(MeshSectionData.RDGPositionBuffer);
				}
				else
				{
					check(false); // Should never happen
					continue;
				}
				if (Buffers != nullptr)
				{
					SetSectionBufferIndex(Parameters, SectionIt, Buffers->SectionBufferIndex);
				}
				else
				{
					// Only support 8 unique different buffer at the moment
					check(UniqueMeshSectionBufferIndex < 8);
					SetMeshSectionBuffers(UniqueMeshSectionBufferIndex, MeshSectionData);

					FMeshSectionBuffers Entry;
					Entry.SectionBufferIndex = UniqueMeshSectionBufferIndex;
					Entry.RDGPositionBuffer = MeshSectionData.RDGPositionBuffer;
					Entry.PositionBuffer = MeshSectionData.PositionBuffer;

					if (MeshSectionData.PositionBuffer)
					{
						UniqueMeshSectionBuffers.Add(MeshSectionData.PositionBuffer, Entry);
					}
					else if (MeshSectionData.RDGPositionBuffer)
					{
						UniqueMeshSectionBuffersRDG.Add(MeshSectionData.RDGPositionBuffer, Entry);
					}

					SetSectionBufferIndex(Parameters, SectionIt, UniqueMeshSectionBufferIndex);
					++UniqueMeshSectionBufferIndex;
				}

				// Sanity check
				// If one of the input is using RDG position, we expect all mesh sections to use RDG input
				if (bUseRDGPositionBuffer)
				{
					check(MeshSectionData.RDGPositionBuffer != nullptr);
				}
				else if (MeshSectionData.RDGPositionBuffer != nullptr)
				{
					bUseRDGPositionBuffer = true;
				}
			}

			if (MeshData.Sections.Num() > 0)
			{
				for (uint32 Index = UniqueMeshSectionBufferIndex; Index < 8; ++Index)
				{
					SetMeshSectionBuffers(Index, MeshData.Sections[0]);
				}
			}

			if (UniqueMeshSectionBufferIndex == 0)
			{
				return;
			}
			Parameters.MaxVertexCount = MeshData.Sections[0].TotalVertexCount;
			Parameters.MaxSampleCount = RestLODData.SampleCount;
			Parameters.SampleIndicesBuffer = RegisterAsSRV(GraphBuilder, RestLODData.MeshSampleIndicesBuffer);
			Parameters.OutSamplePositionsBuffer = OutBuffer.UAV;

			FHairInitMeshSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHairInitMeshSamplesCS::FParameters>();
			*PassParameters = Parameters;

			FHairInitMeshSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FHairInitMeshSamplesCS::FPositionType>(bUseRDGPositionBuffer ? 1 : 0);

			const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(RestLODData.SampleCount, 128);
			check(DispatchGroupCount.X <= GRHIMaxDispatchThreadGroupsPerDimension.X);
			TShaderMapRef<FHairInitMeshSamplesCS> ComputeShader(ShaderMap, PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("HairStrandsInitMeshSamples"),
				ComputeShader,
				PassParameters,
				DispatchGroupCount);
		}
		GraphBuilder.SetBufferAccessFinal(OutBuffer.Buffer, ERHIAccess::SRVMask);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
class FHairUpdateMeshSamplesCS : public FGlobalShader
{
private:
	DECLARE_GLOBAL_SHADER(FHairUpdateMeshSamplesCS);
	SHADER_USE_PARAMETER_STRUCT(FHairUpdateMeshSamplesCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxSampleCount)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, SampleIndicesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, InterpolationWeightsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SampleRestPositionsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SampleDeformedPositionsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutSampleDeformationsBuffer)
		END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_SAMPLE_UPDATE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairUpdateMeshSamplesCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainCS", SF_Compute);

void AddHairStrandUpdateMeshSamplesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources)
{
	if (LODIndex < 0 || LODIndex >= RestResources->LODs.Num() || LODIndex >= DeformedResources->LODs.Num())
	{
		return;
	}

	FHairStrandsRestRootResource::FLOD& RestLODData = RestResources->LODs[LODIndex];
	FHairStrandsDeformedRootResource::FLOD& DeformedLODData = DeformedResources->LODs[LODIndex];
	check(RestLODData.LODIndex == LODIndex);
	check(DeformedLODData.LODIndex == LODIndex);

	const uint32 SectionCount = MeshData.Sections.Num();
	if (SectionCount > 0 && RestLODData.SampleCount > 0)
	{
		FHairUpdateMeshSamplesCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairUpdateMeshSamplesCS::FParameters>();

		FRDGImportedBuffer OutWeightsBuffer = Register(GraphBuilder, DeformedLODData.GetMeshSampleWeightsBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateUAV);

		Parameters->MaxSampleCount					= RestLODData.SampleCount;
		Parameters->SampleIndicesBuffer				= RegisterAsSRV(GraphBuilder, RestLODData.MeshSampleIndicesBuffer);
		Parameters->InterpolationWeightsBuffer		= RegisterAsSRV(GraphBuilder, RestLODData.MeshInterpolationWeightsBuffer);
		Parameters->SampleRestPositionsBuffer		= RegisterAsSRV(GraphBuilder, RestLODData.RestSamplePositionsBuffer);
		Parameters->SampleDeformedPositionsBuffer	= RegisterAsSRV(GraphBuilder, DeformedLODData.GetDeformedSamplePositionsBuffer(FHairStrandsDeformedRootResource::FLOD::Current));
		Parameters->OutSampleDeformationsBuffer		= OutWeightsBuffer.UAV;

		const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(RestLODData.SampleCount+4, 128);
		check(DispatchGroupCount.X <= GRHIMaxDispatchThreadGroupsPerDimension.X);
		TShaderMapRef<FHairUpdateMeshSamplesCS> ComputeShader(ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HairStrands::UpdateMeshSamples"),
			ComputeShader,
			Parameters,
			DispatchGroupCount);

		GraphBuilder.SetBufferAccessFinal(OutWeightsBuffer.Buffer, ERHIAccess::SRVMask);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////

// Generate follicle mask texture
BEGIN_SHADER_PARAMETER_STRUCT(FHairFollicleMaskParameters, )
	SHADER_PARAMETER(FVector2f, OutputResolution)
	SHADER_PARAMETER(uint32, MaxRootCount)
	SHADER_PARAMETER(uint32, MaxUniqueTriangleIndex)
	SHADER_PARAMETER(uint32, Channel)
	SHADER_PARAMETER(uint32, KernelSizeInPixels)

	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, UniqueTrianglePositionBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RootToUniqueTriangleIndexBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RootBarycentricBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RootUVsBuffer)

	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FHairFollicleMask : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_FOLLICLE_MASK"), 1);
	}

	FHairFollicleMask() = default;
	FHairFollicleMask(const CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) {}
};

class FHairFollicleMaskVS : public FHairFollicleMask
{
	DECLARE_GLOBAL_SHADER(FHairFollicleMaskVS);
	SHADER_USE_PARAMETER_STRUCT(FHairFollicleMaskVS, FHairFollicleMask);

	class FUVType : SHADER_PERMUTATION_INT("PERMUTATION_UV_TYPE", 2);
	using FPermutationDomain = TShaderPermutationDomain<FUVType>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FHairFollicleMaskParameters, Pass)
	END_SHADER_PARAMETER_STRUCT()
};

class FHairFollicleMaskPS : public FHairFollicleMask
{
	DECLARE_GLOBAL_SHADER(FHairFollicleMaskPS);
	SHADER_USE_PARAMETER_STRUCT(FHairFollicleMaskPS, FHairFollicleMask);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FHairFollicleMaskParameters, Pass)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FHairFollicleMaskPS, "/Engine/Private/HairStrands/HairStrandsFollicleMask.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FHairFollicleMaskVS, "/Engine/Private/HairStrands/HairStrandsFollicleMask.usf", "MainVS", SF_Vertex);

static void AddFollicleMaskPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const bool bNeedClear,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const uint32 LODIndex,
	FHairStrandsRestRootResource* RestResources,
	FRDGTextureRef OutTexture)
{
	const uint32 RootCount = RestResources->GetRootCount();
	if (LODIndex >= uint32(RestResources->LODs.Num()) || RootCount == 0)
		return;

	FHairStrandsRestRootResource::FLOD& LODData = RestResources->LODs[LODIndex];
	if (!LODData.RootBarycentricBuffer.Buffer || !LODData.RestUniqueTrianglePositionBuffer.Buffer)
		return;

	const FIntPoint OutputResolution = OutTexture->Desc.Extent;
	FHairFollicleMaskParameters* Parameters = GraphBuilder.AllocParameters<FHairFollicleMaskParameters>();
	Parameters->UniqueTrianglePositionBuffer = RegisterAsSRV(GraphBuilder, LODData.RestUniqueTrianglePositionBuffer);
	Parameters->RootToUniqueTriangleIndexBuffer = RegisterAsSRV(GraphBuilder, LODData.RootToUniqueTriangleIndexBuffer);
	Parameters->RootBarycentricBuffer   = RegisterAsSRV(GraphBuilder, LODData.RootBarycentricBuffer);
	Parameters->RootUVsBuffer = nullptr;
	Parameters->OutputResolution = OutputResolution;
	Parameters->MaxRootCount = RootCount;
	Parameters->MaxUniqueTriangleIndex = RestResources->BulkData.Header.LODs[LODIndex].UniqueTriangleCount;
	Parameters->Channel = FMath::Min(Channel, 3u);
	Parameters->KernelSizeInPixels = FMath::Clamp(KernelSizeInPixels, 2u, 200u);
	Parameters->RenderTargets[0] = FRenderTargetBinding(OutTexture, bNeedClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, 0);

	FHairFollicleMaskVS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairFollicleMaskVS::FUVType>(0); // Mesh UVs

	TShaderMapRef<FHairFollicleMaskVS> VertexShader(ShaderMap, PermutationVector);
	TShaderMapRef<FHairFollicleMaskPS> PixelShader(ShaderMap);
	FHairFollicleMaskVS::FParameters ParametersVS;
	FHairFollicleMaskPS::FParameters ParametersPS;
	ParametersVS.Pass = *Parameters;
	ParametersPS.Pass = *Parameters;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("HairStrands::FollicleMask"),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, ParametersVS, ParametersPS, VertexShader, PixelShader, OutputResolution](FRHICommandList& RHICmdList)
	{

		RHICmdList.SetViewport(0, 0, 0.0f, OutputResolution.X, OutputResolution.Y, 1.0f);

		// Apply additive blending pipeline state.
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Max, BF_SourceColor, BF_DestColor, BO_Max, BF_One, BF_One>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);
		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), ParametersPS);

		// Emit an instanced quad draw call on the order of the number of pixels on the screen.	
		RHICmdList.DrawPrimitive(0, Parameters->MaxRootCount, 1);
	});
}

static void AddFollicleMaskPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const bool bNeedClear,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const uint32 RootCount,
	const FRDGBufferRef RootUVBuffer,
	FRDGTextureRef OutTexture)
{
	const FIntPoint OutputResolution = OutTexture->Desc.Extent;
	FHairFollicleMaskParameters* Parameters = GraphBuilder.AllocParameters<FHairFollicleMaskParameters>();
	Parameters->UniqueTrianglePositionBuffer = nullptr;
	Parameters->RootBarycentricBuffer = nullptr;
	Parameters->RootUVsBuffer = GraphBuilder.CreateSRV(RootUVBuffer, PF_G32R32F);
	Parameters->OutputResolution = OutputResolution;
	Parameters->MaxRootCount = RootCount;
	Parameters->MaxUniqueTriangleIndex = 0;
	Parameters->Channel = FMath::Min(Channel, 3u);
	Parameters->KernelSizeInPixels = FMath::Clamp(KernelSizeInPixels, 2u, 200u);
	Parameters->RenderTargets[0] = FRenderTargetBinding(OutTexture, bNeedClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, 0);

	FHairFollicleMaskVS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairFollicleMaskVS::FUVType>(1); // Groom root's UV

	TShaderMapRef<FHairFollicleMaskVS> VertexShader(ShaderMap, PermutationVector);
	TShaderMapRef<FHairFollicleMaskPS> PixelShader(ShaderMap);
	FHairFollicleMaskVS::FParameters ParametersVS;
	FHairFollicleMaskPS::FParameters ParametersPS;
	ParametersVS.Pass = *Parameters;
	ParametersPS.Pass = *Parameters;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("HairStrands::FollicleMask"),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, ParametersVS, ParametersPS, VertexShader, PixelShader, OutputResolution](FRHICommandList& RHICmdList)
		{

			RHICmdList.SetViewport(0, 0, 0.0f, OutputResolution.X, OutputResolution.Y, 1.0f);

			// Apply additive blending pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Max, BF_SourceColor, BF_DestColor, BO_Max, BF_One, BF_One>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), ParametersPS);

			// Emit an instanced quad draw call on the order of the number of pixels on the screen.	
			RHICmdList.DrawPrimitive(0, Parameters->MaxRootCount, 1);
		});
}

void GenerateFolliculeMask(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const EPixelFormat Format,
	const FIntPoint Resolution,
	const uint32 MipCount,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const int32 LODIndex,
	FHairStrandsRestRootResource* RestResources,
	FRDGTextureRef& OutTexture)
{
	const FLinearColor ClearColor(0.0f, 0.f, 0.f, 0.f);

	bool bClear = OutTexture == nullptr;
	if (OutTexture == nullptr)
	{
		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(Resolution, Format, FClearValueBinding(ClearColor), TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, MipCount);
		OutTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("Hair.FollicleMask"));
	}

	AddFollicleMaskPass(GraphBuilder, ShaderMap, bClear, KernelSizeInPixels, Channel, LODIndex, RestResources, OutTexture);
}

void GenerateFolliculeMask(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const EPixelFormat Format,
	const FIntPoint Resolution,
	const uint32 MipCount,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const TArray<FRDGBufferRef>& RootUVBuffers,
	FRDGTextureRef& OutTexture)
{
	const FLinearColor ClearColor(0.0f, 0.f, 0.f, 0.f);

	bool bClear = OutTexture == nullptr;
	if (OutTexture == nullptr)
	{
		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(Resolution, Format, FClearValueBinding(ClearColor), TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, MipCount);
		OutTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("Hair.FollicleMask"));
	}

	for (const FRDGBufferRef& RootUVBuffer : RootUVBuffers)
	{
		const uint32 RootCount = RootUVBuffer->Desc.NumElements;
		AddFollicleMaskPass(GraphBuilder, ShaderMap, bClear, KernelSizeInPixels, Channel, RootCount, RootUVBuffer, OutTexture);
		bClear = false;
	}
}

class FGenerateMipCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateMipCS);
	SHADER_USE_PARAMETER_STRUCT(FGenerateMipCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER(uint32, SourceMip)
		SHADER_PARAMETER(uint32, TargetMip)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_GENERATE_MIPS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateMipCS, "/Engine/Private/HairStrands/HairStrandsFollicleMask.usf", "MainCS", SF_Compute);

void AddComputeMipsPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FRDGTextureRef& OutTexture)
{
	check(OutTexture->Desc.Extent.X == OutTexture->Desc.Extent.Y);
	const uint32 Resolution = OutTexture->Desc.Extent.X;
	const uint32 MipCount = OutTexture->Desc.NumMips;
	for (uint32 MipIt = 0; MipIt < MipCount - 1; ++MipIt)
	{
		const uint32 SourceMipIndex = MipIt;
		const uint32 TargetMipIndex = MipIt + 1;
		const uint32 TargetResolution = Resolution << TargetMipIndex;

		FGenerateMipCS::FParameters* Parameters = GraphBuilder.AllocParameters<FGenerateMipCS::FParameters>();
		Parameters->InTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(OutTexture, SourceMipIndex));
		Parameters->OutTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutTexture, TargetMipIndex));
		Parameters->Resolution = Resolution;
		Parameters->SourceMip = SourceMipIndex;
		Parameters->TargetMip = TargetMipIndex;
		Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		TShaderMapRef<FGenerateMipCS> ComputeShader(ShaderMap);
		ClearUnusedGraphResources(ComputeShader, Parameters);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("HairStrandsComputeVoxelMip"),
			Parameters,
			ERDGPassFlags::Compute,
			[Parameters, ComputeShader, TargetResolution](FRHICommandList& RHICmdList)
		{
			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(TargetResolution, TargetResolution), FIntPoint(8, 8));
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *Parameters, GroupCount);
		});
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairUpdatePositionOffsetCS : public FGlobalShader
{
public:
private:
	DECLARE_GLOBAL_SHADER(FHairUpdatePositionOffsetCS);
	SHADER_USE_PARAMETER_STRUCT(FHairUpdatePositionOffsetCS, FGlobalShader);

	class FUseGPUOffset : SHADER_PERMUTATION_BOOL("PERMUTATION_USE_GPU_OFFSET");
	class FPrevious : SHADER_PERMUTATION_BOOL("PERMUTATION_PREVIOUS");
	using FPermutationDomain = TShaderPermutationDomain<FUseGPUOffset, FPrevious>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, UpdateType)
		SHADER_PARAMETER(uint32, CardLODIndex)
		SHADER_PARAMETER(uint32, InstanceRegisteredIndex)
		SHADER_PARAMETER(FVector3f, CPUCurrPositionOffset)
		SHADER_PARAMETER(FVector3f, CPUPrevPositionOffset)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, RootTriangleCurrPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, RootTrianglePrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutCurrOffsetBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutPrevOffsetBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_OFFSET_UPDATE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairUpdatePositionOffsetCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainCS", SF_Compute);

void AddHairStrandUpdatePositionOffsetPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	EHairPositionUpdateType UpdateType,
	const int32 InstanceRegisteredIndex,
	const int32 HairLODIndex,
	const int32 MeshLODIndex,
	FHairStrandsDeformedRootResource* DeformedRootResources,
	FHairStrandsDeformedResource* DeformedResources)
{
	if ((DeformedRootResources && MeshLODIndex < 0) || DeformedResources == nullptr)
	{
		return;
	}

	const bool bComputePreviousDeformedPosition = IsHairStrandContinuousDecimationReorderingEnabled();

	FRDGImportedBuffer OutCurrPositionOffsetBuffer = Register(GraphBuilder, DeformedResources->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Current), ERDGImportedBufferFlags::CreateUAV);
	FRDGImportedBuffer OutPrevPositionOffsetBuffer = Register(GraphBuilder, DeformedResources->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Previous), ERDGImportedBufferFlags::CreateUAV);

	FRDGImportedBuffer RootTriangleCurrPositionBuffer;
	FRDGImportedBuffer RootTrianglePrevPositionBuffer;
	if (DeformedRootResources)
	{
		RootTriangleCurrPositionBuffer = Register(GraphBuilder, DeformedRootResources->LODs[MeshLODIndex].GetDeformedUniqueTrianglePositionBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV);
		RootTrianglePrevPositionBuffer = Register(GraphBuilder, DeformedRootResources->LODs[MeshLODIndex].GetDeformedUniqueTrianglePositionBuffer(FHairStrandsDeformedRootResource::FLOD::Previous), ERDGImportedBufferFlags::CreateSRV);
	}

	const bool bUseGPUOffset = DeformedRootResources != nullptr && DeformedRootResources->IsValid() && DeformedRootResources->IsInitialized() && GHairStrandsUseGPUPositionOffset > 0;
	const uint32 CurrOffsetIndex = DeformedResources->GetIndex(FHairStrandsDeformedResource::Current);
	const uint32 PrevOffsetIndex = DeformedResources->GetIndex(FHairStrandsDeformedResource::Previous);

	FHairUpdatePositionOffsetCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHairUpdatePositionOffsetCS::FParameters>();
	PassParameters->UpdateType 				= uint32(UpdateType);
	PassParameters->CardLODIndex			= HairLODIndex;
	PassParameters->InstanceRegisteredIndex = InstanceRegisteredIndex;

	PassParameters->CPUCurrPositionOffset			= (FVector3f)DeformedResources->PositionOffset[CurrOffsetIndex];
	PassParameters->CPUPrevPositionOffset			= (FVector3f)DeformedResources->PositionOffset[PrevOffsetIndex];
	PassParameters->RootTriangleCurrPositionBuffer	= RootTriangleCurrPositionBuffer.SRV;
	PassParameters->RootTrianglePrevPositionBuffer	= RootTrianglePrevPositionBuffer.SRV;
	PassParameters->OutCurrOffsetBuffer				= OutCurrPositionOffsetBuffer.UAV;
	PassParameters->OutPrevOffsetBuffer				= OutPrevPositionOffsetBuffer.UAV;

	FHairUpdatePositionOffsetCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairUpdatePositionOffsetCS::FUseGPUOffset>(bUseGPUOffset);
	PermutationVector.Set<FHairUpdatePositionOffsetCS::FPrevious>(bComputePreviousDeformedPosition);
	TShaderMapRef<FHairUpdatePositionOffsetCS> ComputeShader(ShaderMap, PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::UpdatePositionOffset(%s,%s)", bUseGPUOffset ? TEXT("GPU") : TEXT("CPU"), bComputePreviousDeformedPosition ? TEXT("Current/Previous") : TEXT("Current")),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutCurrPositionOffsetBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutPrevPositionOffsetBuffer.Buffer, ERHIAccess::SRVMask);
}