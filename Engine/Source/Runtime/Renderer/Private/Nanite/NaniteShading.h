// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteSceneProxy.h"

#include "SceneRendering.h"
#include "Stats/Stats.h"

struct FNaniteShadingCommands;

DECLARE_CYCLE_STAT_EXTERN(TEXT("NaniteBasePass]"), STAT_CLP_NaniteBasePass, STATGROUP_ParallelCommandListMarkers, );

namespace Nanite
{

struct FRasterResults;

struct FShadeBinning
{
	FRDGBufferRef ShadingBinMeta  = nullptr;
	FRDGBufferRef ShadingBinArgs  = nullptr;
	FRDGBufferRef ShadingBinData = nullptr;
	FRDGBufferRef ShadingBinStats = nullptr;
};

FShadeBinning ShadeBinning(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntRect InViewRect,
	const FNaniteShadingCommands& ShadingCommands,
	const FRasterResults& RasterResults,
	const TConstArrayView<FRDGTextureRef> ClearTargets
);

void BuildShadingCommands(
	const FScene& Scene,
	const FNaniteShadingPipelines& ShadingPipelines,
	FNaniteShadingCommands& ShadingCommands
);

bool LoadShadingPipeline(
	const FScene& Scene,
	FSceneProxyBase* SceneProxy,
	FSceneProxyBase::FMaterialSection& Section,
	FNaniteShadingPipeline& ShadingPipeline
);

void DispatchBasePass(
	FRDGBuilder& GraphBuilder,
	const FSceneRenderer& SceneRenderer,
	const FSceneTextures& SceneTextures,
	const FRenderTargetBindingSlots& BasePassRenderTargets,
	const FDBufferTextures& DBufferTextures,
	const FScene& Scene,
	const FViewInfo& View,
	const FRasterResults& RasterResults
);

} // Nanite
