// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingScene.h"

#if RHI_RAYTRACING

#include "RayTracingInstanceBufferUtil.h"
#include "RenderCore.h"
#include "RayTracingDefinitions.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RaytracingOptions.h"
#include "PrimitiveSceneProxy.h"
#include "SceneUniformBuffer.h"
#include "SceneRendering.h"
#include "RayTracingInstanceCulling.h"

static TAutoConsoleVariable<int32> CVarRayTracingSceneBuildMode(
	TEXT("r.RayTracing.Scene.BuildMode"),
	1,
	TEXT("Controls the mode in which ray tracing scene is built:\n")
	TEXT(" 0: Fast build\n")
	TEXT(" 1: Fast trace (default)\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

BEGIN_SHADER_PARAMETER_STRUCT(FBuildInstanceBufferPassParams, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, InstanceBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutputStats)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, DebugInstanceGPUSceneIndexBuffer)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
END_SHADER_PARAMETER_STRUCT()

FRayTracingScene::FRayTracingScene()
{

}

FRayTracingScene::~FRayTracingScene()
{
}

FRayTracingSceneWithGeometryInstances FRayTracingScene::BuildInitializationData() const
{
	ERayTracingAccelerationStructureFlags BuildFlags = CVarRayTracingSceneBuildMode.GetValueOnRenderThread()
		? ERayTracingAccelerationStructureFlags::FastTrace
		: ERayTracingAccelerationStructureFlags::FastBuild;

	return CreateRayTracingSceneWithGeometryInstances(Instances, uint8(ERayTracingSceneLayer::NUM), BuildFlags);
}

void FRayTracingScene::Create(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FGPUScene* GPUScene)
{
	CreateWithInitializationData(GraphBuilder, View, GPUScene, BuildInitializationData(), ERDGPassFlags::Compute);
}

void FRayTracingScene::InitPreViewTranslation(const FViewMatrices& ViewMatrices)
{
	PreViewTranslation = FDFVector3(ViewMatrices.GetPreViewTranslation());
}

void FRayTracingScene::CreateWithInitializationData(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FGPUScene* GPUScene, FRayTracingSceneWithGeometryInstances SceneWithGeometryInstances, ERDGPassFlags ComputePassFlags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingScene::CreateWithInitializationData);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RayTracingScene_CreateWithInitializationData);

	// Round up buffer sizes to some multiple to avoid pathological growth reallocations.
	static constexpr uint32 AllocationGranularity = 8 * 1024;
	static constexpr uint64 BufferAllocationGranularity = 16 * 1024 * 1024;

	bUsedThisFrame = true;

	FRHICommandListBase& RHICmdList = GraphBuilder.RHICmdList;

	static const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);

	checkf(SceneWithGeometryInstances.Scene.IsValid(), 
		TEXT("Ray tracing scene RHI object is expected to have been created by BuildInitializationData() or CreateRayTracingSceneWithGeometryInstances()"));

	RayTracingSceneRHI = SceneWithGeometryInstances.Scene;
	check(NumSegments == SceneWithGeometryInstances.TotalNumSegments);

	const FRayTracingSceneInitializer2& SceneInitializer = RayTracingSceneRHI->GetInitializer();

	const uint32 NumNativeInstances = SceneWithGeometryInstances.NumNativeGPUSceneInstances + SceneWithGeometryInstances.NumNativeCPUInstances;
	const uint32 NumNativeInstancesAligned = FMath::DivideAndRoundUp(FMath::Max(NumNativeInstances, 1U), AllocationGranularity) * AllocationGranularity;
	const uint32 NumTransformsAligned = FMath::DivideAndRoundUp(FMath::Max(SceneWithGeometryInstances.NumNativeCPUInstances, 1U), AllocationGranularity) * AllocationGranularity;

	FRayTracingAccelerationStructureSize SizeInfo = RayTracingSceneRHI->GetSizeInfo();
	SizeInfo.ResultSize = FMath::DivideAndRoundUp(FMath::Max(SizeInfo.ResultSize, 1ull), BufferAllocationGranularity) * BufferAllocationGranularity;

	// Allocate GPU buffer if current one is too small or significantly larger than what we need.
	if (!RayTracingScenePooledBuffer.IsValid()
		|| SizeInfo.ResultSize > RayTracingScenePooledBuffer->GetSize()
		|| SizeInfo.ResultSize < RayTracingScenePooledBuffer->GetSize() / 2)
	{
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(1, uint32(SizeInfo.ResultSize));
		Desc.Usage = EBufferUsageFlags::AccelerationStructure;

		RayTracingScenePooledBuffer = AllocatePooledBuffer(Desc, TEXT("FRayTracingScene::SceneBuffer"));
	}
	RayTracingSceneBufferRDG = GraphBuilder.RegisterExternalBuffer(RayTracingScenePooledBuffer);

	LayerSRVs.SetNum(NumLayers);
	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		LayerSRVs[LayerIndex] = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayTracingSceneBufferRDG, RayTracingSceneRHI->GetLayerBufferOffset(LayerIndex), 0));
	}

	{
		const uint64 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
		FRDGBufferDesc ScratchBufferDesc;
		ScratchBufferDesc.Usage = EBufferUsageFlags::RayTracingScratch | EBufferUsageFlags::StructuredBuffer;
		ScratchBufferDesc.BytesPerElement = uint32(ScratchAlignment);
		ScratchBufferDesc.NumElements = uint32(FMath::DivideAndRoundUp(SizeInfo.BuildScratchSize, ScratchAlignment));

		BuildScratchBuffer = GraphBuilder.CreateBuffer(ScratchBufferDesc, TEXT("FRayTracingScene::ScratchBuffer"));
	}

	{
		FRDGBufferDesc InstanceBufferDesc;
		InstanceBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
		InstanceBufferDesc.BytesPerElement = GRHIRayTracingInstanceDescriptorSize;
		InstanceBufferDesc.NumElements = NumNativeInstancesAligned;

		InstanceBuffer = GraphBuilder.CreateBuffer(InstanceBufferDesc, TEXT("FRayTracingScene::InstanceBuffer"));
	}

	{
		// Round to PoT to avoid resizing too often
		const uint32 NumGeometries = FMath::RoundUpToPowerOfTwo(SceneInitializer.ReferencedGeometries.Num());
		const uint32 AccelerationStructureAddressesBufferSize = NumGeometries * sizeof(FRayTracingAccelerationStructureAddress);

		if (AccelerationStructureAddressesBuffer.NumBytes < AccelerationStructureAddressesBufferSize)
		{
			// Need to pass "BUF_MultiGPUAllocate", as virtual addresses are different per GPU
			AccelerationStructureAddressesBuffer.Initialize(
				GraphBuilder.RHICmdList, TEXT("FRayTracingScene::AccelerationStructureAddressesBuffer"), AccelerationStructureAddressesBufferSize, BUF_Volatile | BUF_MultiGPUAllocate);
		}
	}

	{
		// Create/resize instance upload buffer (if necessary)
		const uint32 UploadBufferSize = NumNativeInstancesAligned * sizeof(FRayTracingInstanceDescriptorInput);

		if (!InstanceUploadBuffer.IsValid()
			|| UploadBufferSize > InstanceUploadBuffer->GetSize()
			|| UploadBufferSize < InstanceUploadBuffer->GetSize() / 2)
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FRayTracingScene::InstanceUploadBuffer"));
			InstanceUploadBuffer = RHICmdList.CreateStructuredBuffer(sizeof(FRayTracingInstanceDescriptorInput), UploadBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
			InstanceUploadSRV = RHICmdList.CreateShaderResourceView(InstanceUploadBuffer);
		}
	}

	{
		const uint32 UploadBufferSize = NumTransformsAligned * sizeof(FVector4f) * 3;

		// Create/resize transform upload buffer (if necessary)
		if (!TransformUploadBuffer.IsValid()
			|| UploadBufferSize > TransformUploadBuffer->GetSize()
			|| UploadBufferSize < TransformUploadBuffer->GetSize() / 2)
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FRayTracingScene::TransformUploadBuffer"));
			TransformUploadBuffer = RHICmdList.CreateStructuredBuffer(sizeof(FVector4f), UploadBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
			TransformUploadSRV = RHICmdList.CreateShaderResourceView(TransformUploadBuffer);
		}
	}

#if STATS
	FRDGBufferDesc OutputStatsBufferDesc(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1));
	OutputStatsBufferDesc.Usage |= BUF_SourceCopy;

	FRDGBufferRef OutputStatsBuffer = GraphBuilder.CreateBuffer(OutputStatsBufferDesc, TEXT("FRayTracingScene::OutputStatsBuffer"));
	FRDGBufferUAVRef OutputStatsBufferUAV = GraphBuilder.CreateUAV(OutputStatsBuffer);
	AddClearUAVPass(GraphBuilder, OutputStatsBufferUAV, 0);
#endif

	FRDGBufferUAVRef DebugInstanceGPUSceneIndexBufferUAV = nullptr;
	if (bNeedsDebugInstanceGPUSceneIndexBuffer)
	{
		FRDGBufferDesc DebugInstanceGPUSceneIndexBufferDesc;
		DebugInstanceGPUSceneIndexBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
		DebugInstanceGPUSceneIndexBufferDesc.BytesPerElement = sizeof(uint32);
		DebugInstanceGPUSceneIndexBufferDesc.NumElements = FMath::Max(NumNativeInstances, 1u);

		DebugInstanceGPUSceneIndexBuffer = GraphBuilder.CreateBuffer(DebugInstanceGPUSceneIndexBufferDesc, TEXT("FRayTracingScene::DebugInstanceGPUSceneIndexBuffer"));
		DebugInstanceGPUSceneIndexBufferUAV = GraphBuilder.CreateUAV(DebugInstanceGPUSceneIndexBuffer);

		AddClearUAVPass(GraphBuilder, DebugInstanceGPUSceneIndexBufferUAV, 0xFFFFFFFF);
	}

	if (InstancesDebugData.Num() > 0 && NumNativeInstances > 0)
	{
		// Create InstanceDebugBuffer (one entry per instance in TLAS)
		// This requires replicating the data in InstancesDebugData (one entry per FRayTracingGeometryInstance) according to NumTransforms in each geometry instance

		check(InstancesDebugData.Num() == Instances.Num());

		TArrayView<uint32> LayerBaseIndices = MakeArrayView(GraphBuilder.AllocPODArray<uint32>(NumLayers), NumLayers);
		LayerBaseIndices[0] = 0;

		for (uint32 LayerIndex = 1; LayerIndex < NumLayers; ++LayerIndex)
		{
			LayerBaseIndices[LayerIndex] = LayerBaseIndices[LayerIndex - 1] + SceneInitializer.NumNativeInstancesPerLayer[LayerIndex - 1];
		}

		// make a copy of SceneWithGeometryInstances.BaseInstancePrefixSum that can be passed to the RDG setup tasks below
		TArray<uint32, SceneRenderingAllocator> BaseInstancePrefixSum = GraphBuilder.AllocArray<uint32>();
		BaseInstancePrefixSum = SceneWithGeometryInstances.BaseInstancePrefixSum;

		FRDGUploadData<FRayTracingInstanceDebugData> UploadData(GraphBuilder, NumNativeInstances);

		{
			const uint32 NumItems = InstancesDebugData.Num();

			// Distribute work evenly to the available task graph workers based on NumItems.
			const uint32 TargetItemsPerTask = 512;
			const uint32 NumThreads = FMath::Min(FTaskGraphInterface::Get().GetNumWorkerThreads(), CVarRHICmdWidth.GetValueOnRenderThread());
			const uint32 NumTasks = FMath::Min(NumThreads, FMath::DivideAndRoundUp(NumItems, TargetItemsPerTask));
			const uint32 NumItemsPerTask = FMath::DivideAndRoundUp(NumItems, NumTasks);

			for (uint32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
			{
				const uint32 TaskFirstItemIndex = TaskIndex * NumItemsPerTask;
				const uint32 TaskNumItems = FMath::Min(NumItemsPerTask, NumItems - TaskFirstItemIndex);

				TConstArrayView<FRayTracingGeometryInstance> TaskInstancesData(Instances.GetData() + TaskFirstItemIndex, TaskNumItems);
				TConstArrayView<FRayTracingInstanceDebugData> TaskInstancesDebugData(InstancesDebugData.GetData() + TaskFirstItemIndex, TaskNumItems);
				TConstArrayView<uint32> TaskBaseInstancePrefixSum(BaseInstancePrefixSum.GetData() + TaskFirstItemIndex, TaskNumItems);

				GraphBuilder.AddSetupTask([UploadData, TaskInstancesDebugData, TaskInstancesData, TaskBaseInstancePrefixSum, LayerBaseIndices]()
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FillRayTracingInstanceDebugBuffer);

						for (int32 Index = 0; Index < TaskInstancesDebugData.Num(); ++Index)
						{
							const FRayTracingGeometryInstance& SceneInstance = TaskInstancesData[Index];
							const uint32 BaseInstanceIndex = TaskBaseInstancePrefixSum[Index];
							const uint32 LayerBaseIndex = LayerBaseIndices[SceneInstance.LayerIndex];

							for (uint32 TransformIndex = 0; TransformIndex < SceneInstance.NumTransforms; ++TransformIndex)
							{
								// write data in the same order used in InstanceBuffer used to build TLAS / InstanceIndex() in hit shaders
								UploadData[LayerBaseIndex + BaseInstanceIndex + TransformIndex] = TaskInstancesDebugData[Index];
							}
						}
					});
			}
		}

		InstanceDebugBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("FRayTracingScene::InstanceDebugData"), UploadData);
	}

	if (NumNativeInstances > 0)
	{
		const uint32 InstanceUploadBytes = NumNativeInstances * sizeof(FRayTracingInstanceDescriptorInput);
		const uint32 TransformUploadBytes = SceneWithGeometryInstances.NumNativeCPUInstances * 3 * sizeof(FVector4f);

		FRayTracingInstanceDescriptorInput* InstanceUploadData = (FRayTracingInstanceDescriptorInput*)RHICmdList.LockBuffer(InstanceUploadBuffer, 0, InstanceUploadBytes, RLM_WriteOnly);
		FVector4f* TransformUploadData = (TransformUploadBytes > 0) ? (FVector4f*)RHICmdList.LockBuffer(TransformUploadBuffer, 0, TransformUploadBytes, RLM_WriteOnly) : nullptr;

		// Fill instance upload buffer on separate thread since results are only needed in RHI thread
		GraphBuilder.AddSetupTask(
			[InstanceUploadData = MakeArrayView(InstanceUploadData, NumNativeInstances),
			TransformUploadData = MakeArrayView(TransformUploadData, SceneWithGeometryInstances.NumNativeCPUInstances * 3),
			NumNativeGPUSceneInstances = SceneWithGeometryInstances.NumNativeGPUSceneInstances,
			NumNativeCPUInstances = SceneWithGeometryInstances.NumNativeCPUInstances,
			Instances = MakeArrayView(Instances),
			InstanceGeometryIndices = MoveTemp(SceneWithGeometryInstances.InstanceGeometryIndices),
			BaseUploadBufferOffsets = MoveTemp(SceneWithGeometryInstances.BaseUploadBufferOffsets),
			BaseInstancePrefixSum = MoveTemp(SceneWithGeometryInstances.BaseInstancePrefixSum),
			RayTracingSceneRHI = RayTracingSceneRHI,
			PreViewTranslation = View.ViewMatrices.GetPreViewTranslation()]()
		{
			FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
			FillRayTracingInstanceUploadBuffer(
				RayTracingSceneRHI,
				PreViewTranslation,
				Instances,
				InstanceGeometryIndices,
				BaseUploadBufferOffsets,
				BaseInstancePrefixSum,
				NumNativeGPUSceneInstances,
				NumNativeCPUInstances,
				InstanceUploadData,
				TransformUploadData);
		});

		FBuildInstanceBufferPassParams* PassParams = GraphBuilder.AllocParameters<FBuildInstanceBufferPassParams>();
		PassParams->InstanceBuffer = GraphBuilder.CreateUAV(InstanceBuffer);
		PassParams->DebugInstanceGPUSceneIndexBuffer = DebugInstanceGPUSceneIndexBufferUAV;
		PassParams->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);

#if STATS
		PassParams->OutputStats = OutputStatsBufferUAV;
#endif

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("RayTracingBuildInstanceBuffer"),
			PassParams,
			ComputePassFlags,
			[PassParams,
			this,
			GPUScene,
			PreViewTranslation = PreViewTranslation,
			&SceneInitializer,
			NumNativeGPUSceneInstances = SceneWithGeometryInstances.NumNativeGPUSceneInstances,
			NumNativeCPUInstances = SceneWithGeometryInstances.NumNativeCPUInstances,
			CullingParameters = View.RayTracingCullingParameters
			](FRHICommandList& RHICmdList)
			{
				RHICmdList.UnlockBuffer(InstanceUploadBuffer);

				if (NumNativeCPUInstances > 0)
				{
					RHICmdList.UnlockBuffer(TransformUploadBuffer);
				}

				for (uint32 GPUIndex : RHICmdList.GetGPUMask())
				{
					FRayTracingAccelerationStructureAddress* AddressesPtr = (FRayTracingAccelerationStructureAddress*)RHICmdList.LockBufferMGPU(
						AccelerationStructureAddressesBuffer.Buffer,
						GPUIndex,
						0,
						SceneInitializer.ReferencedGeometries.Num() * sizeof(FRayTracingAccelerationStructureAddress), RLM_WriteOnly);

					RHICmdList.EnqueueLambda([AddressesPtr, &SceneInitializer, GPUIndex](FRHICommandListBase&)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(GetAccelerationStructuresAddresses);

						for (int32 GeometryIndex = 0; GeometryIndex < SceneInitializer.ReferencedGeometries.Num(); ++GeometryIndex)
						{
							AddressesPtr[GeometryIndex] = SceneInitializer.ReferencedGeometries[GeometryIndex]->GetAccelerationStructureAddress(GPUIndex);
						}
					});

					RHICmdList.UnlockBufferMGPU(AccelerationStructureAddressesBuffer.Buffer, GPUIndex);
				}

				BuildRayTracingInstanceBuffer(
					RHICmdList,
					GPUScene,
					PreViewTranslation,
					PassParams->InstanceBuffer->GetRHI(),
					InstanceUploadSRV,
					AccelerationStructureAddressesBuffer.SRV,
					TransformUploadSRV,
					NumNativeGPUSceneInstances,
					NumNativeCPUInstances,
					CullingParameters.bUseInstanceCulling ? &CullingParameters : nullptr,
					PassParams->OutputStats ? PassParams->OutputStats->GetRHI() : nullptr,
					PassParams->DebugInstanceGPUSceneIndexBuffer ? PassParams->DebugInstanceGPUSceneIndexBuffer->GetRHI() : nullptr);
			});
	}

#if STATS
	// readback
	{
		//  if necessary create readback buffers
		if (StatsReadbackBuffers.IsEmpty())
		{
			StatsReadbackBuffers.SetNum(MaxReadbackBuffers);

			for (uint32 Index = 0; Index < MaxReadbackBuffers; ++Index)
			{
				StatsReadbackBuffers[Index] = new FRHIGPUBufferReadback(TEXT("FRayTracingScene::StatsReadbackBuffer"));
			}
		}
		
		// copy stats to readback buffer
		{
			AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("FRayTracingScene::StatsReadback"), OutputStatsBuffer,
				[ReadbackBuffer = StatsReadbackBuffers[StatsReadbackBuffersWriteIndex], OutputStatsBuffer](FRHICommandList& RHICmdList)
				{
					ReadbackBuffer->EnqueueCopy(RHICmdList, OutputStatsBuffer->GetRHI(), 0u);
				});

			StatsReadbackBuffersWriteIndex = (StatsReadbackBuffersWriteIndex + 1u) % MaxReadbackBuffers;
			StatsReadbackBuffersNumPending = FMath::Min(StatsReadbackBuffersNumPending + 1u, MaxReadbackBuffers);
		}

		// process ready results
		while (StatsReadbackBuffersNumPending > 0)
		{
			uint32 Index = (StatsReadbackBuffersWriteIndex + MaxReadbackBuffers - StatsReadbackBuffersNumPending) % MaxReadbackBuffers;
			FRHIGPUBufferReadback* ReadbackBuffer = StatsReadbackBuffers[Index];
			if (ReadbackBuffer->IsReady())
			{
				StatsReadbackBuffersNumPending--;

				auto ReadbackBufferPtr = (const uint32*)ReadbackBuffer->Lock(sizeof(uint32));

				NumActiveInstances = ReadbackBufferPtr[0];

				ReadbackBuffer->Unlock();
			}
			else
			{
				break;
			}
		}

		SET_DWORD_STAT(STAT_RayTracingTotalInstances, NumNativeInstances);
		SET_DWORD_STAT(STAT_RayTracingActiveInstances, FMath::Min(NumActiveInstances, NumNativeInstances));
	}
#endif
}

BEGIN_SHADER_PARAMETER_STRUCT(FRayTracingSceneBuildPassParams, )
	RDG_BUFFER_ACCESS(ScratchBuffer, ERHIAccess::UAVCompute)
	RDG_BUFFER_ACCESS(InstanceBuffer, ERHIAccess::SRVCompute)
	RDG_BUFFER_ACCESS(TLASBuffer, ERHIAccess::BVHWrite)

	RDG_BUFFER_ACCESS(DynamicGeometryScratchBuffer, ERHIAccess::UAVCompute)
END_SHADER_PARAMETER_STRUCT()

void FRayTracingScene::Build(FRDGBuilder& GraphBuilder, ERDGPassFlags ComputePassFlags, FRDGBufferRef DynamicGeometryScratchBuffer)
{
	FRayTracingSceneBuildPassParams* PassParams = GraphBuilder.AllocParameters<FRayTracingSceneBuildPassParams>();
	PassParams->ScratchBuffer = BuildScratchBuffer;
	PassParams->InstanceBuffer = InstanceBuffer;
	PassParams->TLASBuffer = GetBufferChecked();
	PassParams->DynamicGeometryScratchBuffer = DynamicGeometryScratchBuffer; // TODO: Is this necessary?

	GraphBuilder.AddPass(RDG_EVENT_NAME("RayTracingBuildScene"), PassParams, ComputePassFlags,
		[this, PassParams](FRHICommandList& RHICmdList)
		{
			FRayTracingSceneBuildParams BuildParams;
			BuildParams.Scene = RayTracingSceneRHI;
			BuildParams.ScratchBuffer = PassParams->ScratchBuffer->GetRHI();
			BuildParams.ScratchBufferOffset = 0;
			BuildParams.InstanceBuffer = PassParams->InstanceBuffer->GetRHI();
			BuildParams.InstanceBufferOffset = 0;

			RHICmdList.BindAccelerationStructureMemory(RayTracingSceneRHI, PassParams->TLASBuffer->GetRHI(), 0);
			RHICmdList.BuildAccelerationStructure(BuildParams);
		});
}

bool FRayTracingScene::IsCreated() const
{
	return RayTracingSceneRHI.IsValid();
}

FRHIRayTracingScene* FRayTracingScene::GetRHIRayTracingScene() const
{
	return RayTracingSceneRHI.GetReference();
}

FRHIRayTracingScene* FRayTracingScene::GetRHIRayTracingSceneChecked() const
{
	FRHIRayTracingScene* Result = GetRHIRayTracingScene();
	checkf(Result, TEXT("Ray tracing scene was not created. Perhaps Create() was not called."));
	return Result;
}

FRDGBufferRef FRayTracingScene::GetBufferChecked() const
{
	checkf(RayTracingSceneBufferRDG, TEXT("Ray tracing scene buffer was not created. Perhaps Create() was not called."));
	return RayTracingSceneBufferRDG;
}

FShaderResourceViewRHIRef FRayTracingScene::CreateLayerViewRHI(FRHICommandListBase& RHICmdList, ERayTracingSceneLayer Layer) const
{
	const uint8 LayerIndex = uint8(Layer);
	checkf(RayTracingScenePooledBuffer, TEXT("Ray tracing scene was not created.Perhaps Create() was not called."));
	return RHICmdList.CreateShaderResourceView(FShaderResourceViewInitializer(RayTracingScenePooledBuffer->GetRHI(), RayTracingSceneRHI->GetLayerBufferOffset(LayerIndex), 0));
}

FRDGBufferSRVRef FRayTracingScene::GetLayerView(ERayTracingSceneLayer Layer) const
{
	checkf(LayerSRVs[uint8(Layer)], TEXT("Ray tracing scene SRV was not created. Perhaps Create() was not called."));
	return LayerSRVs[uint8(Layer)];
}

uint32 FRayTracingScene::AddInstance(FRayTracingGeometryInstance Instance, const FPrimitiveSceneProxy* Proxy, bool bDynamic)
{
	FRHIRayTracingGeometry* GeometryRHI = Instance.GeometryRHI;

	const uint32 InstanceIndex = Instances.Add(MoveTemp(Instance));

	if (bInstanceDebugDataEnabled)
	{
		FRayTracingInstanceDebugData& InstanceDebugData = InstancesDebugData.AddDefaulted_GetRef();
		InstanceDebugData.Flags = bDynamic ? 1 : 0;
		InstanceDebugData.GeometryAddress = uint64(GeometryRHI);

		if (Proxy)
		{
			InstanceDebugData.ProxyHash = Proxy->GetTypeHash();
		}

		check(Instances.Num() == InstancesDebugData.Num());
	}

	return InstanceIndex;
}

uint32 FRayTracingScene::AddInstancesUninitialized(uint32 NumInstances)
{
	const uint32 OldNum = Instances.AddUninitialized(NumInstances);

	if (bInstanceDebugDataEnabled)
	{
		InstancesDebugData.AddUninitialized(NumInstances);

		check(Instances.Num() == InstancesDebugData.Num());
	}

	return OldNum;
}

void FRayTracingScene::SetInstance(uint32 InstanceIndex, FRayTracingGeometryInstance InInstance, const FPrimitiveSceneProxy* Proxy, bool bDynamic)
{
	FRHIRayTracingGeometry* GeometryRHI = InInstance.GeometryRHI;

	FRayTracingGeometryInstance* Instance = &Instances[InstanceIndex];
	new (Instance) FRayTracingGeometryInstance(MoveTemp(InInstance));

	if (bInstanceDebugDataEnabled)
	{
		FRayTracingInstanceDebugData InstanceDebugData;
		InstanceDebugData.Flags = bDynamic ? 1 : 0;
		InstanceDebugData.GeometryAddress = uint64(GeometryRHI);

		if (Proxy)
		{
			InstanceDebugData.ProxyHash = Proxy->GetTypeHash();
		}

		InstancesDebugData[InstanceIndex] = InstanceDebugData;
	}
}

void FRayTracingScene::Reset(bool bInInstanceDebugDataEnabled)
{
	Instances.Reset();
	InstancesDebugData.Reset();
	CallableCommands.Reset();
	UniformBuffers.Reset();
	GeometriesToBuild.Reset();
	UsedCoarseMeshStreamingHandles.Reset();

	NumSegments = 0;
	NumMissShaderSlots = 1;
	NumCallableShaderSlots = 0;

	Allocator.Flush();

	RayTracingSceneRHI = nullptr;
	RayTracingSceneBufferRDG = nullptr;

	InstanceBuffer = nullptr;
	BuildScratchBuffer = nullptr;
	InstanceDebugBuffer = nullptr;
	DebugInstanceGPUSceneIndexBuffer = nullptr;

	bInstanceDebugDataEnabled = bInInstanceDebugDataEnabled;
}

void FRayTracingScene::EndFrame()
{
	Reset(false);

	// Release the resources if ray tracing wasn't used
	if (!bUsedThisFrame)
	{
		Instances.Empty();
		InstancesDebugData.Empty();
		CallableCommands.Empty();
		UniformBuffers.Empty();
		GeometriesToBuild.Empty();
		UsedCoarseMeshStreamingHandles.Empty();

		RayTracingScenePooledBuffer = nullptr;

#if STATS
		for (auto& ReadbackBuffer : StatsReadbackBuffers)
		{
			delete ReadbackBuffer;
		}

		StatsReadbackBuffers.Empty();

		StatsReadbackBuffersWriteIndex = 0;
		StatsReadbackBuffersNumPending = 0;

		NumActiveInstances = 0;
#endif
	}

	bUsedThisFrame = false;
}

#endif // RHI_RAYTRACING
