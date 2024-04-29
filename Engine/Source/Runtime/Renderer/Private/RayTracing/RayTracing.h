// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"
#include "RayTracingInstance.h"
#include "MeshPassProcessor.h"
#include "RenderGraphDefinitions.h"

enum class EDiffuseIndirectMethod;
enum class EReflectionsMethod;
class FRayTracingScene;
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
		FGlobalDynamicReadBuffer& InDynamicReadBuffer,
		FSceneRenderingBulkObjectAllocator& InBulkAllocator,
		FRelevantPrimitiveList& RelevantPrimitiveList);

	bool ShouldExcludeDecals();
}

#endif // RHI_RAYTRACING