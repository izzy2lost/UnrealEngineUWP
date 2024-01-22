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
			QuadTreePositionParameter.Bind(ParameterMap, TEXT("QuadTreePosition"));
			LODMorphingEnabledParameter.Bind(ParameterMap, TEXT("bLODMorphingEnabled"));
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

		if (bIndirectDraws)
		{
			if (VertexStreams.Num() > 0)
			{
				const int32 NumBuffers = bWithWaterSelectionSupport ? 4 : 3;
				FRHIBuffer* InstanceVertexBuffers[] = 
				{ 
					WaterMeshUserData->IndirectInstanceData0, 
					WaterMeshUserData->IndirectInstanceData1,
					WaterMeshUserData->IndirectInstanceData2,
					WaterMeshUserData->IndirectInstanceData3,
				};
				for (int32 i = 0; i < NumBuffers; ++i)
				{
					FVertexInputStream* InstanceInputStream = VertexStreams.FindByPredicate([i](const FVertexInputStream& InStream) { return InStream.StreamIndex == 1 + i; });
					check(InstanceInputStream);

					// Bind vertex buffer
					check(InstanceVertexBuffers[i]);
					InstanceInputStream->VertexBuffer = InstanceVertexBuffers[i];
				}
			}

			const FVector PreViewTranslation = View->ViewMatrices.GetPreViewTranslation();
			ShaderBindings.Add(QuadTreePositionParameter, FVector3f(PreViewTranslation + VertexFactory->GetQuadTreePositionWS()));

			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Water.WaterMesh.LODMorphEnabled"));
			const bool bLODMorphingEnabled = CVar && CVar->GetValueOnRenderThread() != 0;
			ShaderBindings.Add(LODMorphingEnabledParameter, bLODMorphingEnabled ? 1 : 0);
		}
		else if (VertexStreams.Num() > 0)
		{
			for (int32 i = 0; i < WaterInstanceDataBuffersType::NumBuffers; ++i)
			{
				FVertexInputStream* InstanceInputStream = VertexStreams.FindByPredicate([i](const FVertexInputStream& InStream) { return InStream.StreamIndex == i + 1; });
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
	}

private:
	LAYOUT_FIELD(FShaderParameter, QuadTreePositionParameter);
	LAYOUT_FIELD(FShaderParameter, LODMorphingEnabledParameter);
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
