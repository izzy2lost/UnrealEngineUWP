// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterVertexFactory.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "MeshDrawShaderBindings.h"
#include "MeshBatch.h"
#include "MeshMaterialShader.h"
#include "RenderUtils.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FWaterVertexFactoryParameters, "WaterVF");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FWaterVertexFactoryRaytracingParameters, "WaterRaytracingVF");

/**
 * Shader parameters for water vertex factory.
 */
template <bool bWithWaterSelectionSupport, bool bIndirectDraws>
class TWaterVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	using WaterVertexFactoryShaderParametersType = TWaterVertexFactoryShaderParameters<bWithWaterSelectionSupport, bIndirectDraws>;
	DECLARE_TYPE_LAYOUT(WaterVertexFactoryShaderParametersType, NonVirtual);

public:
	using WaterVertexFactoryType = TWaterVertexFactory<bWithWaterSelectionSupport, bIndirectDraws>;
	using WaterMeshUserDataType = TWaterMeshUserData<bWithWaterSelectionSupport>;
	using WaterInstanceDataBuffersType = TWaterInstanceDataBuffers<bWithWaterSelectionSupport>;

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		if (bIndirectDraws)
		{
			InstanceDataOffsetsBufferParameter.Bind(ParameterMap, TEXT("InstanceDataOffsetsBuffer"));
			InstanceData0BufferParameter.Bind(ParameterMap, TEXT("InstanceData0Buffer"));
			InstanceData1BufferParameter.Bind(ParameterMap, TEXT("InstanceData1Buffer"));
			if (bWithWaterSelectionSupport)
			{
				InstanceData2BufferParameter.Bind(ParameterMap, TEXT("InstanceData2Buffer"));
			}
			
			DrawBucketIndexParameter.Bind(ParameterMap, TEXT("DrawBucketIndex"));
		}
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const class FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const class FVertexFactory* InVertexFactory,
		const struct FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		WaterVertexFactoryType* VertexFactory = (WaterVertexFactoryType*)InVertexFactory;

		const WaterMeshUserDataType* WaterMeshUserData = (const WaterMeshUserDataType*)BatchElement.UserData;

		const WaterInstanceDataBuffersType* InstanceDataBuffers = WaterMeshUserData->InstanceDataBuffers;

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FWaterVertexFactoryParameters>(), VertexFactory->GetWaterVertexFactoryUniformBuffer(WaterMeshUserData->RenderGroupType));

#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			ShaderBindings.Add(Shader->GetUniformBufferParameter<FWaterVertexFactoryRaytracingParameters>(), WaterMeshUserData->WaterVertexFactoryRaytracingVFUniformBuffer);
		}
#endif

		if (VertexStreams.Num() > 0 && !bIndirectDraws)
		{
			for (int32 i = 0; i < WaterInstanceDataBuffersType::NumBuffers; ++i)
			{
				FVertexInputStream* InstanceInputStream = VertexStreams.FindByPredicate([i](const FVertexInputStream& InStream) { return InStream.StreamIndex == i+1; });
				check(InstanceInputStream);
				
				// Bind vertex buffer
				check(InstanceDataBuffers->GetBuffer(i));
				InstanceInputStream->VertexBuffer = InstanceDataBuffers->GetBuffer(i);
			}

			const int32 InstanceOffsetValue = BatchElement.UserIndex;
			if (InstanceOffsetValue > 0)
			{
				VertexFactory->OffsetInstanceStreams(InstanceOffsetValue, InputStreamType, VertexStreams);
			}
		}

		if (bIndirectDraws)
		{
			const uint32 DrawBucketIndex = BatchElement.UserIndex;
			ShaderBindings.Add(InstanceDataOffsetsBufferParameter, WaterMeshUserData->IndirectInstanceDataOffsets);
			ShaderBindings.Add(InstanceData0BufferParameter, WaterMeshUserData->IndirectInstanceData0);
			ShaderBindings.Add(InstanceData1BufferParameter, WaterMeshUserData->IndirectInstanceData1);
			if (bWithWaterSelectionSupport)
			{
				ShaderBindings.Add(InstanceData2BufferParameter, WaterMeshUserData->IndirectInstanceData2);
			}
			ShaderBindings.Add(DrawBucketIndexParameter, DrawBucketIndex);
		}
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, InstanceDataOffsetsBufferParameter);
	LAYOUT_FIELD(FShaderResourceParameter, InstanceData0BufferParameter);
	LAYOUT_FIELD(FShaderResourceParameter, InstanceData1BufferParameter);
	LAYOUT_FIELD(FShaderResourceParameter, InstanceData2BufferParameter);
	LAYOUT_FIELD(FShaderParameter, DrawBucketIndexParameter);
};

// ----------------------------------------------------------------------------------

using FWaterVertexFactoryParametersNoSelectionNoIndirect = TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ false, /*bIndirectDraws = */ false>;
using FWaterVertexFactoryParametersNoSelectionWithIndirect = TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ false, /*bIndirectDraws = */ true>;
using FWaterVertexFactoryNoSelectionNoIndirect = TWaterVertexFactory</*bWithWaterSelectionSupport = */ false, /*bIndirectDraws = */ false>;
using FWaterVertexFactoryNoSelectionWithIndirect = TWaterVertexFactory</*bWithWaterSelectionSupport = */ false, /*bIndirectDraws = */ true>;

// Always implement the basic vertex factory so that it's there for both editor and non-editor builds :
IMPLEMENT_TEMPLATE_TYPE_LAYOUT(template<>, FWaterVertexFactoryParametersNoSelectionNoIndirect);
IMPLEMENT_TEMPLATE_TYPE_LAYOUT(template<>, FWaterVertexFactoryParametersNoSelectionWithIndirect);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FWaterVertexFactoryNoSelectionNoIndirect, SF_Vertex, FWaterVertexFactoryParametersNoSelectionNoIndirect);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FWaterVertexFactoryNoSelectionWithIndirect, SF_Vertex, FWaterVertexFactoryParametersNoSelectionWithIndirect);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FWaterVertexFactoryNoSelectionNoIndirect, SF_Compute, FWaterVertexFactoryParametersNoSelectionNoIndirect);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FWaterVertexFactoryNoSelectionNoIndirect, SF_RayHitGroup, FWaterVertexFactoryParametersNoSelectionNoIndirect);
#endif // RHI_RAYTRACING
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, FWaterVertexFactoryNoSelectionNoIndirect, "/Plugin/Water/Private/WaterMeshVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsPSOPrecaching
);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, FWaterVertexFactoryNoSelectionWithIndirect, "/Plugin/Water/Private/WaterMeshVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsPSOPrecaching
);

#if WITH_WATER_SELECTION_SUPPORT

using FWaterVertexFactoryParametersWithSelectionNoIndirect = TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ true, /*bIndirectDraws = */ false>;
using FWaterVertexFactoryParametersWithSelectionWithIndirect = TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ true, /*bIndirectDraws = */ true>;
using FWaterVertexFactoryWithSelectionNoIndirect = TWaterVertexFactory</*bWithWaterSelectionSupport = */ true, /*bIndirectDraws = */ false>;
using FWaterVertexFactoryWithSelectionWithIndirect = TWaterVertexFactory</*bWithWaterSelectionSupport = */ true, /*bIndirectDraws = */ true>;

// In editor builds, also implement the vertex factory that supports water selection:
IMPLEMENT_TEMPLATE_TYPE_LAYOUT(template<>, FWaterVertexFactoryParametersWithSelectionNoIndirect);
IMPLEMENT_TEMPLATE_TYPE_LAYOUT(template<>, FWaterVertexFactoryParametersWithSelectionWithIndirect);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FWaterVertexFactoryWithSelectionNoIndirect, SF_Vertex, FWaterVertexFactoryParametersWithSelectionNoIndirect);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FWaterVertexFactoryWithSelectionWithIndirect, SF_Vertex, FWaterVertexFactoryParametersWithSelectionWithIndirect);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FWaterVertexFactoryWithSelectionNoIndirect, SF_Compute, FWaterVertexFactoryParametersWithSelectionNoIndirect);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FWaterVertexFactoryWithSelectionNoIndirect, SF_RayHitGroup, FWaterVertexFactoryParametersWithSelectionNoIndirect);
#endif
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, FWaterVertexFactoryWithSelectionNoIndirect, "/Plugin/Water/Private/WaterMeshVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsPSOPrecaching
);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, FWaterVertexFactoryWithSelectionWithIndirect, "/Plugin/Water/Private/WaterMeshVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsPSOPrecaching
);

#endif // WITH_WATER_SELECTION_SUPPORT

const FVertexFactoryType* GetWaterVertexFactoryType(bool bWithWaterSelectionSupport, bool bIndirectDraws)
{
#if WITH_WATER_SELECTION_SUPPORT
	if (bWithWaterSelectionSupport)
	{
		if (bIndirectDraws)
		{
			return &TWaterVertexFactory</*bWithWaterSelectionSupport = */ true, /*bIndirectDraws = */ true>::StaticType;
		}
		else
		{
			return &TWaterVertexFactory</*bWithWaterSelectionSupport = */ true, /*bIndirectDraws = */ false>::StaticType;
		}
	}
	else
#endif // WITH_WATER_SELECTION_SUPPORT
	{
		check(!bWithWaterSelectionSupport);
		if (bIndirectDraws)
		{
			return &TWaterVertexFactory</*bWithWaterSelectionSupport = */ false, /*bIndirectDraws = */ true>::StaticType;
		}
		else
		{
			return &TWaterVertexFactory</*bWithWaterSelectionSupport = */ false, /*bIndirectDraws = */ false>::StaticType;
		}
		
	}
}
