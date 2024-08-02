// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Metal RHI public headers.
#include "MetalThirdParty.h"
#include "MetalState.h"
#include "MetalResources.h"
#include "MetalViewport.h"
#include "RHICore.h"

class FMetalDeviceContext;
class FMetalCommandBufferFence;

#if PLATFORM_VISIONOS
namespace MetalRHIVisionOS
{
    struct BeginRenderingImmersiveParams;
    struct PresentImmersiveParams;
}
#endif

/** The interface RHI command context. */
class FMetalRHICommandContext : public IRHICommandContext
{
public:
	FMetalRHICommandContext(class FMetalProfiler* InProfiler, FMetalDeviceContext* WrapContext);
	virtual ~FMetalRHICommandContext();

	/** Get the internal context */
	FORCEINLINE FMetalDeviceContext& GetInternalContext() const { return *Context; }
	
	/** Get the profiler pointer */
	FORCEINLINE class FMetalProfiler* GetProfiler() const { return Profiler; }

	virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState) override;
	
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	
	virtual void RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	
	// Useful when used with geometry shader (emit polygons to different viewports), otherwise SetViewPort() is simpler
	// @param Count >0
	// @param Data must not be 0
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	
	/** Clears a UAV to the multi-component value provided. */
	virtual void RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values) final override;
	virtual void RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values) final override;
	
	virtual void RHICopyTexture(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FRHICopyTextureInfo& CopyInfo) final override;
	virtual void RHICopyBufferRegion(FRHIBuffer* DstBufferRHI, uint64 DstOffset, FRHIBuffer* SrcBufferRHI, uint64 SrcOffset, uint64 NumBytes) final override;

    virtual void RHICalibrateTimers(FRHITimestampCalibrationQuery* CalibrationQuery) final override;
    
	virtual void RHIBeginRenderQuery(FRHIRenderQuery* RenderQuery) final override;
	virtual void RHIEndRenderQuery(FRHIRenderQuery* RenderQuery) final override;
	
	void RHIBeginOcclusionQueryBatch(uint32 NumQueriesInBatch);
	void RHIEndOcclusionQueryBatch();

	virtual void RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask) final override;
	
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginDrawingViewport(FRHIViewport* Viewport, FRHITexture* RenderTargetRHI) override;
	
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync) override;
	
	virtual void RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset) final override;

	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) final override;

	virtual void RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ) final override;
	
	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	
	virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState) final override;
	
	virtual void RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers) final override;

	virtual void RHISetShaderParameters(FRHIGraphicsShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override;
	virtual void RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters) final override;

	virtual void RHISetStencilRef(uint32 StencilRef) final override;

	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) final override;

	void SetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget);
	
	void SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo);
	
	virtual void RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	
	virtual void RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	
	virtual void RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	
	// @param NumPrimitives need to be >0
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	
	virtual void RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;

#if PLATFORM_SUPPORTS_MESH_SHADERS
    virtual void RHIDispatchMeshShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
    virtual void RHIDispatchIndirectMeshShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
#endif

	/**
	* Sets Depth Bounds Testing with the given min/max depth.
	* @param MinDepth	The minimum depth for depth bounds test
	* @param MaxDepth	The maximum depth for depth bounds test.
	*					The valid values for fMinDepth and fMaxDepth are such that 0 <= fMinDepth <= fMaxDepth <= 1
	*/
	virtual void RHISetDepthBounds(float MinDepth, float MaxDepth) final override;

#if WITH_RHI_BREADCRUMBS
	virtual void RHIBeginBreadcrumbGPU(FRHIBreadcrumbNode* Breadcrumb) final override;
	virtual void RHIEndBreadcrumbGPU(FRHIBreadcrumbNode* Breadcrumb) final override;
#endif

	virtual void RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* DestinationStagingBufferRHI, uint32 Offset, uint32 NumBytes) final override;
	virtual void RHIWriteGPUFence(FRHIGPUFence* FenceRHI) final override;

	virtual void RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions);
	virtual void RHIEndTransitions(TArrayView<const FRHITransition*> Transitions);

	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName) final override;
	virtual void RHIEndRenderPass() final override;
	
	virtual void RHINextSubpass() final override;

#if METAL_RHI_RAYTRACING
	virtual void RHIBindAccelerationStructureMemory(FRHIRayTracingScene* Scene, FRHIBuffer* Buffer, uint32 BufferOffset) final override;
	virtual void RHIBuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange) final override;
	virtual void RHIBuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams) final override;
	virtual void RHIClearRayTracingBindings(FRHIRayTracingScene* Scene) final override;
	virtual void RHIClearShaderBindingTable(FRHIShaderBindingTable* SBT) final override;
	virtual void RHIRayTraceDispatch(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIRayTracingScene* Scene,
		const FRayTracingShaderBindings& GlobalResourceBindings,
		uint32 Width, uint32 Height) final override;
	virtual void RHIRayTraceDispatch(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIShaderBindingTable* SBT, const FRayTracingShaderBindings& GlobalResourceBindings,
		uint32 Width, uint32 Height) final override;
	virtual void RHIRayTraceDispatchIndirect(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIRayTracingScene* Scene,
		const FRayTracingShaderBindings& GlobalResourceBindings,
		FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIRayTraceDispatchIndirect(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
		FRHIShaderBindingTable* SBT, const FRayTracingShaderBindings& GlobalResourceBindings,
		FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHISetRayTracingBindings(
		FRHIRayTracingScene* Scene, FRHIRayTracingPipelineState* Pipeline,
		uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings,
		ERayTracingBindingType BindingType) final override;
	virtual void RHISetBindingsOnShaderBindingTable(
		FRHIShaderBindingTable* SBT, FRHIRayTracingPipelineState* Pipeline,
		uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings,
		ERayTracingBindingType BindingType) final override;
#endif // METAL_RHI_RAYTRACING

#if PLATFORM_VISIONOS
    void BeginRenderingImmersive(const MetalRHIVisionOS::BeginRenderingImmersiveParams& Params);
    cp_frame_t SwiftFrame = nullptr;
#endif // PLATFORM_VISIONOS
    void SetCustomPresentViewport(FRHIViewport* Viewport) { CustomPresentViewport = Viewport; }
    FRHIViewport* CustomPresentViewport = nullptr;

	void BeginRecursiveCommand()
	{
		// Nothing to do
	}
    
    inline const TArray<FRHIUniformBuffer*>& GetStaticUniformBuffers() const
    {
        return GlobalUniformBuffers;
    }
protected:
	
	/** Context implementation details. */
	FMetalDeviceContext* Context = nullptr;
	
	/** Occlusion query batch fence */
	TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> CommandBufferFence;
	
	/** Profiling implementation details. */
	class FMetalProfiler* Profiler = nullptr;
	
	/** Some local variables to track the pending primitive information used in RHIEnd*UP functions */
	FMetalBuffer PendingVertexBuffer;
	uint32 PendingVertexDataStride = 0;
	
	FMetalBuffer PendingIndexBuffer;
	uint32 PendingIndexDataStride = 0;
	
	uint32 PendingPrimitiveType = 0;
	uint32 PendingNumPrimitives = 0;

	void ResolveTexture(UE::RHICore::FResolveTextureInfo Info);

	TArray<FRHIUniformBuffer*> GlobalUniformBuffers;

private:
	void RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);
};

class FMetalRHIImmediateCommandContext : public FMetalRHICommandContext
{
public:
	FMetalRHIImmediateCommandContext(class FMetalProfiler* InProfiler, FMetalDeviceContext* WrapContext);
	
protected:
	friend class FMetalDynamicRHI;
};

struct FMetalContextArray : public TRHIPipelineArray<FMetalRHICommandContext*>
{
	FMetalContextArray(FRHIContextArray const& Contexts);
};
