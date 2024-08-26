// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RendererInterface.h"
#include "RenderGraphDefinitions.h"
#include "RayTracingDefinitions.h"
#include "RHIDefinitions.h"
#include "ShaderCore.h"

enum class EDiffuseIndirectMethod;
enum class EReflectionsMethod;
class FRayTracingScene;
class FRayTracingShaderBindingTable;
class FScene;
class FViewInfo;
class FViewFamilyInfo;
class FGlobalDynamicReadBuffer;

// Settings controlling ray tracing instance caching
namespace RayTracing
{
	struct FSceneOptions
	{
		bool bTranslucentGeometry = true;

		FSceneOptions(FScene& Scene,
			const FViewFamilyInfo& ViewFamily,
			FViewInfo& View,
			EDiffuseIndirectMethod DiffuseIndirectMethod,
			EReflectionsMethod ReflectionsMethod,
			RayTracing::FSceneOptions& SceneOptions);
	};
};

#if RHI_RAYTRACING

namespace RayTracing
{
	struct FRelevantPrimitiveList;

	void OnRenderBegin(FScene& Scene, TArray<FViewInfo>& Views, const FViewFamilyInfo& ViewFamily);

	FRelevantPrimitiveList* CreateRelevantPrimitiveList(FSceneRenderingBulkObjectAllocator& InAllocator);

	// Get shader resource table desc used for all raytracing shaders which is shared between all shaders in the RTPSO
	const FShaderBindingLayout* GetShaderBindingLayout(EShaderPlatform ShaderPlatform);

	// Setup the runtime static uniform buffer bindings on the command list if enabled
	TOptional<FScopedUniformBufferStaticBindings> BindStaticUniformBufferBindings(const FViewInfo& View, FRHIUniformBuffer* SceneUniformBuffer, FRHICommandList& RHICmdList);
	
	// Iterates over Scene's PrimitiveSceneProxies and extracts ones that are relevant for ray tracing.
	// This function can run on any thread.
	void GatherRelevantPrimitives(FScene& Scene, const FViewInfo& View, FRelevantPrimitiveList& OutRelevantPrimitiveList);

	// Fills RayTracingScene instance list for the given View and adds relevant ray tracing data to the view. Does not reset previous scene contents.
	// This function must run on render thread
	bool GatherWorldInstancesForView(
		FRDGBuilder& GraphBuilder,
		FScene& Scene,
		const FViewFamilyInfo& ViewFamily,
		FViewInfo& View,
		EDiffuseIndirectMethod DiffuseIndirectMethod,
		EReflectionsMethod ReflectionsMethod,
		FRayTracingScene& RayTracingScene,
		FRayTracingShaderBindingTable& RayTracingSBT,
		FGlobalDynamicReadBuffer& InDynamicReadBuffer,
		FSceneRenderingBulkObjectAllocator& InBulkAllocator,
		FRelevantPrimitiveList& RelevantPrimitiveList);

	bool ShouldExcludeDecals();
}

#endif // RHI_RAYTRACING