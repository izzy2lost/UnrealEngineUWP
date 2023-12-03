// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HairStrandsMeshProjection.h"
#include "CachedGeometry.h"

struct FCachedGeometry;
class FSkeletalMeshSceneProxy;
class FGlobalShaderMap;
class FRDGBuilder;
class FGeometryCacheSceneProxy;
struct FHairStrandsRootBulkData;

FHairStrandsProjectionMeshData::FSection ConvertMeshSection(const FCachedGeometry& InCachedGeometry, const FCachedGeometry::Section& In);

void BuildCacheGeometry(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap, 
	const FSkeletalMeshSceneProxy* Proxy,
	const FHairStrandsRootBulkData* RootBulkData,
	const bool bOutputTriangleData,
	FCachedGeometry& OutCachedGeometry);

void BuildCacheGeometry(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap, 
	const FGeometryCacheSceneProxy* Proxy,
	const bool bOutputTriangleData,
	FCachedGeometry& OutCachedGeometry);