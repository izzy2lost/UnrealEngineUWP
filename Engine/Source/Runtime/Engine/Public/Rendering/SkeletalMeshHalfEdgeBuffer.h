// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "Containers/DynamicRHIResourceArray.h"

class FSkeletalMeshLODRenderData;
class FRHIShaderResourceView;

namespace SkeletalMeshHalfEdgeUtility
{
	void ENGINE_API BuildHalfEdgeBuffers(const FSkeletalMeshLODRenderData& InLodRenderData, TResourceArray<int32>& OutVertexToEdge, TResourceArray<int32>& OutEdgeToTwinEdge);
}

/*
 * Render resource containing the half edge buffers. 
 */ 
class FSkeletalMeshHalfEdgeBuffer : public FRenderResource
{
public:
	void Init(const FSkeletalMeshLODRenderData& InLodRenderData);
		
	void ENGINE_API InitRHI(FRHICommandListBase& RHICmdList) override;

	void ENGINE_API ReleaseRHI() override;

	bool IsCPUDataValid() const;
	
	void CleanUp();

	int32 GetResourceSize() const;
	
	void Serialize(FArchive& Ar);
	
	friend class FSkeletalMeshLODRenderData;
	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshHalfEdgeBuffer& MorphTargetVertexInfoBuffers);

	FRHIShaderResourceView* GetVertexToEdgeBufferSRV() const
	{
		return VertexToEdgeBufferSRV;
	}

	FRHIShaderResourceView* GetEdgeToTwinEdgeBufferSRV() const
	{
		return EdgeToTwinEdgeBufferSRV;
	}

private:
	TResourceArray<int32> VertexToEdgeData;
	TResourceArray<int32> EdgeToTwinEdgeData;
	
	FBufferRHIRef VertexToEdgeBufferRHI;
	FShaderResourceViewRHIRef VertexToEdgeBufferSRV;		
	FBufferRHIRef EdgeToTwinEdgeBufferRHI;
	FShaderResourceViewRHIRef EdgeToTwinEdgeBufferSRV;	
};