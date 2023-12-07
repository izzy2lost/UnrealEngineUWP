// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/ArrayView.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"

#if RHI_RAYTRACING

class FRayTracingGeometry;
class FRHIComputeCommandList;
enum class EAccelerationStructureBuildMode;
enum class ERTAccelerationStructureBuildPriority;

class IRayTracingGeometryManager
{
public:

	using BuildRequestIndex = int32;
	using RayTracingGeometryHandle = int32;

	virtual ~IRayTracingGeometryManager() = default;

	RENDERCORE_API virtual BuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode) = 0;

	BuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority)
	{
		return RequestBuildAccelerationStructure(InGeometry, InPriority, EAccelerationStructureBuildMode::Build);
	}

	RENDERCORE_API virtual void RemoveBuildRequest(BuildRequestIndex InRequestIndex) = 0;
	RENDERCORE_API virtual void BoostPriority(BuildRequestIndex InRequestIndex, float InBoostValue) = 0;
	RENDERCORE_API virtual void ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries) = 0;
	RENDERCORE_API virtual void ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll = false) = 0;

	RENDERCORE_API virtual RayTracingGeometryHandle RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry) = 0;
	RENDERCORE_API virtual void ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle Handle) = 0;

	RENDERCORE_API virtual void Tick(FRHICommandList& RHICmdList) = 0;
};

extern RENDERCORE_API IRayTracingGeometryManager* GRayTracingGeometryManager;

#endif // RHI_RAYTRACING
