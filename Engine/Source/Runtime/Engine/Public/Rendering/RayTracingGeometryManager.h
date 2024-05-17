// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RayTracingGeometryManagerInterface.h"

#include "Containers/SparseArray.h"
#include "Containers/Map.h"

#if RHI_RAYTRACING

class FPrimitiveSceneProxy;
class UStaticMesh;

class FRayTracingGeometryManager : public IRayTracingGeometryManager
{
public:

	ENGINE_API virtual ~FRayTracingGeometryManager();

	ENGINE_API virtual BuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode) override;

	ENGINE_API virtual void RemoveBuildRequest(BuildRequestIndex InRequestIndex) override;
	ENGINE_API virtual void BoostPriority(BuildRequestIndex InRequestIndex, float InBoostValue) override;
	ENGINE_API virtual void ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries) override;
	ENGINE_API virtual void ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll = false) override;

	ENGINE_API virtual RayTracingGeometryHandle RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry) override;
	ENGINE_API virtual void ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle Handle) override;

	ENGINE_API virtual RayTracing::GeometryGroupHandle RegisterRayTracingGeometryGroup(uint32 NumLODs) override;
	ENGINE_API virtual void ReleaseRayTracingGeometryGroup(RayTracing::GeometryGroupHandle Handle) override;

	ENGINE_API virtual void RefreshRegisteredGeometry(RayTracingGeometryHandle Handle) override;

	ENGINE_API virtual void PreRender() override;
	ENGINE_API virtual void Tick(FRHICommandList& RHICmdList) override;

	ENGINE_API void RegisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle);
	ENGINE_API void UnregisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle);

	ENGINE_API virtual void RequestUpdateCachedRenderState(RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle) override;

	ENGINE_API void AddReferencedGeometry(const FRayTracingGeometry* Geometry);
	ENGINE_API void AddReferencedGeometryGroups(const TSet<RayTracing::GeometryGroupHandle>& GeometryGroups);

#if DO_CHECK
	ENGINE_API bool IsGeometryReferenced(const FRayTracingGeometry* Geometry) const;
	ENGINE_API bool IsGeometryGroupReferenced(RayTracing::GeometryGroupHandle GeometryGroup) const;
#endif

private:

	struct FBuildRequest
	{
		BuildRequestIndex RequestIndex = INDEX_NONE;

		float BuildPriority = 0.0f;
		FRayTracingGeometry* Owner;
		EAccelerationStructureBuildMode BuildMode;

		// TODO: Implement use-after-free checks in BuildRequestIndex using some bits to identify generation
	};

	void SetupBuildParams(const FBuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams, bool bRemoveFromRequestArray = true);

	void ReleaseRayTracingGeometryGroupReference(RayTracing::GeometryGroupHandle Handle);

	FCriticalSection RequestCS;

	TSparseArray<FBuildRequest> GeometryBuildRequests;

	// Working array with all active build build params in the RHI
	TArray<FBuildRequest> SortedRequests;
	TArray<FRayTracingGeometryBuildParams> BuildParams;

	// Operations such as registering geometry/groups can be done from different render command pipes (eg: SkeletalMesh)
	// so need to use critical section in relevant functions
	FCriticalSection MainCS;

	struct FRayTracingGeometryGroup
	{
		TArray<FRayTracingGeometry*> Geometries;

		TSet<FPrimitiveSceneProxy*> ProxiesWithCachedRayTracingState;

		// Due to the way we batch release FRenderResource and SceneProxies, 
		// ReleaseRayTracingGeometryGroup(...) can end up being called before all FRayTracingGeometry and SceneProxies are actually released.
		// To deal with this, we keep track of whether the group is still referenced and only release the group handle once all references are released.
		uint32 NumReferences = 0;

		// TODO: Implement use-after-free checks in RayTracing::GeometryGroupHandle using some bits to identify generation
	};

	struct FRegisteredGeometry
	{
		FRayTracingGeometry* Geometry = nullptr;
		uint32 Size = 0;
	};

	TSparseArray<FRayTracingGeometryGroup> RegisteredGroups;

	// Used for keeping track of geometries when ray tracing is dynamic
	TSparseArray<FRegisteredGeometry> RegisteredGeometries;

	TSet<FRayTracingGeometry*> ResidentGeometries;
	uint64 TotalResidentSize = 0;

	TSet<RayTracingGeometryHandle> ReferencedGeometryHandles;
	TSet<RayTracing::GeometryGroupHandle> ReferencedGeometryGroups;

	bool bRenderedFrame = false;
};

#endif // RHI_RAYTRACING
