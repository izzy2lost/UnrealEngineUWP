// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/SparseArray.h"
#include "Containers/ArrayView.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"

#if RHI_RAYTRACING

class FRayTracingGeometry;
class FRHIComputeCommandList;
enum class EAccelerationStructureBuildMode;
enum class ERTAccelerationStructureBuildPriority;

class FRayTracingGeometryManager
{
public:

	using BuildRequestIndex = int32;
	using RayTracingGeometryHandle = int32;

	FRayTracingGeometryManager() {}
	~FRayTracingGeometryManager() {}
	BuildRequestIndex RequestBuildAccelerationStructure(FRHICommandList& RHICmdList, FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority)
	{
		return RequestBuildAccelerationStructure(RHICmdList, InGeometry, InPriority, EAccelerationStructureBuildMode::Build);
	}	
	RENDERCORE_API BuildRequestIndex RequestBuildAccelerationStructure(FRHICommandList& RHICmdList, FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode);

	UE_DEPRECATED(5.4, "RequestBuildAccelerationStructure requires a command list.")
	BuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority)
	{
		return RequestBuildAccelerationStructure(FRHICommandListImmediate::Get(), InGeometry, InPriority, EAccelerationStructureBuildMode::Build);
	}

	UE_DEPRECATED(5.4, "RequestBuildAccelerationStructure requires a command list.")
	BuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode)
	{
		return RequestBuildAccelerationStructure(FRHICommandListImmediate::Get(), InGeometry, InPriority, InBuildMode);
	}

	RENDERCORE_API void RemoveBuildRequest(BuildRequestIndex InRequestIndex);
	RENDERCORE_API void BoostPriority(BuildRequestIndex InRequestIndex, float InBoostValue);
	RENDERCORE_API void ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries);
	RENDERCORE_API void ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll = false);

	RENDERCORE_API void Tick(FRHICommandList& RHICmdList, bool bHasRayTracingEnableChanged);

	UE_DEPRECATED(5.4, "Tick requires a command list.")
	void Tick(bool bHasRayTracingEnableChanged)
	{
		Tick(FRHICommandListImmediate::Get(), bHasRayTracingEnableChanged);
	}

	RENDERCORE_API RayTracingGeometryHandle RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry);
	RENDERCORE_API void ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle Handle);
private:

	struct BuildRequest
	{
		BuildRequestIndex RequestIndex = INDEX_NONE;

		float BuildPriority = 0.0f;
		FRayTracingGeometry* Owner;
		EAccelerationStructureBuildMode BuildMode;
	};

	RENDERCORE_API void SetupBuildParams(const BuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams);

	FCriticalSection RequestCS;

	TSparseArray<BuildRequest> GeometryBuildRequests;

	// Used for keeping track of geometries when ray tracing is dynamic
	TSparseArray<FRayTracingGeometry*> RegisteredGeometries;

	// Working array with all active build build params in the RHI
	TArray<BuildRequest> SortedRequests;
	TArray<FRayTracingGeometryBuildParams> BuildParams;
};

extern RENDERCORE_API FRayTracingGeometryManager GRayTracingGeometryManager;

#endif // RHI_RAYTRACING
