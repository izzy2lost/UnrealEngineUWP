// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RayTracingGeometryManagerInterface.h"

#include "Containers/SparseArray.h"

#if RHI_RAYTRACING

class FRayTracingGeometry;
class FRHIComputeCommandList;
enum class EAccelerationStructureBuildMode;
enum class ERTAccelerationStructureBuildPriority;

class FRayTracingGeometryManager : public IRayTracingGeometryManager
{
public:

	ENGINE_API virtual ~FRayTracingGeometryManager();

	ENGINE_API virtual BuildRequestIndex RequestBuildAccelerationStructure(FRHICommandList& RHICmdList, FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode) override;

	ENGINE_API virtual void RemoveBuildRequest(BuildRequestIndex InRequestIndex) override;
	ENGINE_API virtual void BoostPriority(BuildRequestIndex InRequestIndex, float InBoostValue) override;
	ENGINE_API virtual void ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries) override;
	ENGINE_API virtual void ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll = false) override;

	ENGINE_API virtual RayTracingGeometryHandle RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry) override;
	ENGINE_API virtual void ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle Handle) override;

	ENGINE_API virtual void Tick(FRHICommandList& RHICmdList, bool bHasRayTracingEnableChanged) override;

private:

	struct BuildRequest
	{
		BuildRequestIndex RequestIndex = INDEX_NONE;

		float BuildPriority = 0.0f;
		FRayTracingGeometry* Owner;
		EAccelerationStructureBuildMode BuildMode;
	};

	void SetupBuildParams(const BuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams);

	FCriticalSection RequestCS;

	TSparseArray<BuildRequest> GeometryBuildRequests;

	// Used for keeping track of geometries when ray tracing is dynamic
	TSparseArray<FRayTracingGeometry*> RegisteredGeometries;

	// Working array with all active build build params in the RHI
	TArray<BuildRequest> SortedRequests;
	TArray<FRayTracingGeometryBuildParams> BuildParams;
};

#endif // RHI_RAYTRACING
