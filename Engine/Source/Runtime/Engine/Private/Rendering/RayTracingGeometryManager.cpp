// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/RayTracingGeometryManager.h"

#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "ComponentRecreateRenderStateContext.h"

#include "RHIResources.h"
#include "RHICommandList.h"

#include "RayTracingGeometry.h"
#include "RenderUtils.h"

#include "Serialization/MemoryReader.h"

#if RHI_RAYTRACING

static bool bHasRayTracingEnableChanged = false;
static TAutoConsoleVariable<int32> CVarRayTracingEnable(
	TEXT("r.RayTracing.Enable"),
	1,
	TEXT("Whether ray tracing is enabled at runtime.\n")
	TEXT("If r.RayTracing.EnableOnDemand is enabled, ray tracing can be toggled on/off at runtime. Otherwise this is only checked during initialization."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			FGlobalComponentRecreateRenderStateContext Context;
			ENQUEUE_RENDER_COMMAND(RayTracingToggledCmd)(
				[](FRHICommandListImmediate&)
				{
					bHasRayTracingEnableChanged = true;
				}
			);
		}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarRayTracingUseReferenceBasedResidency(
	TEXT("r.RayTracing.UseReferenceBasedResidency"),
	false,
	TEXT("(EXPERIMENTAL) Whether raytracing geometries should be resident or evicted based on whether they're referenced in TLAS"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			FGlobalComponentRecreateRenderStateContext Context;
			ENQUEUE_RENDER_COMMAND(RayTracingToggledCmd)(
				[](FRHICommandListImmediate&)
				{
					bHasRayTracingEnableChanged = true;
				}
			);
		}),
	ECVF_ReadOnly
);

static int32 GRayTracingStreamingMaxPendingRequests = 128;
static FAutoConsoleVariableRef CVarNaniteStreamingMaxPendingRequests(
	TEXT("r.RayTracing.Streaming.MaxPendingRequests"),
	GRayTracingStreamingMaxPendingRequests,
	TEXT("Maximum number of requests that can be pending streaming."),
	ECVF_ReadOnly
);

static int32 GRayTracingMaxBuiltPrimitivesPerFrame = -1;
static FAutoConsoleVariableRef CVarRayTracingMaxBuiltPrimitivesPerFrame(
	TEXT("r.RayTracing.Geometry.MaxBuiltPrimitivesPerFrame"),
	GRayTracingMaxBuiltPrimitivesPerFrame,
	TEXT("Sets the ray tracing acceleration structure build budget in terms of maximum number of triangles per frame (<= 0 then disabled and all acceleration structures are build immediatly - default)"),
	ECVF_RenderThreadSafe
);

static float GRayTracingPendingBuildPriorityBoostPerFrame = 0.001f;
static FAutoConsoleVariableRef CVarRayTracingPendingBuildPriorityBoostPerFrame(
	TEXT("r.RayTracing.Geometry.PendingBuildPriorityBoostPerFrame"),
	GRayTracingPendingBuildPriorityBoostPerFrame,
	TEXT("Increment the priority for all pending build requests which are not scheduled that frame (0.001 - default)"),
	ECVF_RenderThreadSafe
);

DECLARE_STATS_GROUP(TEXT("Ray Tracing Geometry"), STATGROUP_RayTracingGeometry, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Geometry Count"), STAT_RayTracingGeometryCount, STATGROUP_RayTracingGeometry);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Geometry Group Count"), STAT_RayTracingGeometryGroupCount, STATGROUP_RayTracingGeometry);

DECLARE_MEMORY_STAT(TEXT("Resident Memory"), STAT_RayTracingGeometryResidentMemory, STATGROUP_RayTracingGeometry);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Pending Builds"), STAT_RayTracingPendingBuilds, STATGROUP_RayTracingGeometry);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Pending Build Primitives"), STAT_RayTracingPendingBuildPrimitives, STATGROUP_RayTracingGeometry);


FRayTracingGeometryManager::FRayTracingGeometryManager()
{
	StreamingRequests.SetNum(GRayTracingStreamingMaxPendingRequests);
}

FRayTracingGeometryManager::~FRayTracingGeometryManager()
{
	check(GeometryBuildRequests.IsEmpty());
	check(RegisteredGeometries.IsEmpty());

	check(RegisteredGroups.IsEmpty());
}

static float GetInitialBuildPriority(ERTAccelerationStructureBuildPriority InBuildPriority)
{
	switch (InBuildPriority)
	{
	case ERTAccelerationStructureBuildPriority::Immediate:	return 1.0f;
	case ERTAccelerationStructureBuildPriority::High:		return 0.5f;
	case ERTAccelerationStructureBuildPriority::Normal:		return 0.24f;
	case ERTAccelerationStructureBuildPriority::Low:		return 0.01f;
	case ERTAccelerationStructureBuildPriority::Skip:
	default:
	{
		checkNoEntry();
		return 0.0f;
	}
	}
}

FRayTracingGeometryManager::BuildRequestIndex FRayTracingGeometryManager::RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode)
{
	FBuildRequest Request;
	Request.BuildPriority = GetInitialBuildPriority(InPriority);
	Request.Owner = InGeometry;
	Request.BuildMode = EAccelerationStructureBuildMode::Build;

	FScopeLock ScopeLock(&RequestCS);
	BuildRequestIndex RequestIndex = GeometryBuildRequests.Add(Request);
	GeometryBuildRequests[RequestIndex].RequestIndex = RequestIndex;

	INC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	INC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, InGeometry->Initializer.TotalPrimitiveCount);

	return RequestIndex;
}

void FRayTracingGeometryManager::RemoveBuildRequest(BuildRequestIndex InRequestIndex)
{
	FScopeLock ScopeLock(&RequestCS);

	DEC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	DEC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, GeometryBuildRequests[InRequestIndex].Owner->Initializer.TotalPrimitiveCount);

	GeometryBuildRequests.RemoveAt(InRequestIndex);
}

RayTracing::GeometryGroupHandle FRayTracingGeometryManager::RegisterRayTracingGeometryGroup(uint32 NumLODs, uint32 CurrentFirstLODIdx)
{
	FScopeLock ScopeLock(&MainCS);

	FRayTracingGeometryGroup Group;
	Group.Geometries.AddDefaulted(NumLODs);
	Group.NumReferences = 1;
	Group.CurrentFirstLODIdx = CurrentFirstLODIdx;

	RayTracing::GeometryGroupHandle Handle = RegisteredGroups.Add(MoveTemp(Group));

	INC_DWORD_STAT(STAT_RayTracingGeometryGroupCount);

	return Handle;
}

void FRayTracingGeometryManager::ReleaseRayTracingGeometryGroup(RayTracing::GeometryGroupHandle Handle)
{
	FScopeLock ScopeLock(&MainCS);

	check(RegisteredGroups.IsValidIndex(Handle));

	ReleaseRayTracingGeometryGroupReference(Handle);
}

void FRayTracingGeometryManager::ReleaseRayTracingGeometryGroupReference(RayTracing::GeometryGroupHandle Handle)
{
	FRayTracingGeometryGroup& Group = RegisteredGroups[Handle];

	--Group.NumReferences;

	if (Group.NumReferences == 0)
	{
		for (FRayTracingGeometry* Geometry : Group.Geometries)
		{
			checkf(Geometry == nullptr, TEXT("All FRayTracingGeometry in a group must be unregistered before releasing the group."));
		}

		check(Group.ProxiesWithCachedRayTracingState.IsEmpty());

		RegisteredGroups.RemoveAt(Handle);
		ReferencedGeometryGroups.Remove(Handle);

		DEC_DWORD_STAT(STAT_RayTracingGeometryGroupCount);
	}
}

FRayTracingGeometryManager::RayTracingGeometryHandle FRayTracingGeometryManager::RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry)
{
	check(InGeometry);

	FScopeLock ScopeLock(&MainCS);

	RayTracingGeometryHandle Handle = RegisteredGeometries.Add({});

	FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Handle];
	RegisteredGeometry.Geometry = InGeometry;

	if (InGeometry->GroupHandle != INDEX_NONE)
	{
		checkf(RegisteredGroups.IsValidIndex(InGeometry->GroupHandle), TEXT("FRayTracingGeometry.GroupHandle must be valid"));

		FRayTracingGeometryGroup& Group = RegisteredGroups[InGeometry->GroupHandle];

		checkf(InGeometry->LODIndex >= 0 && InGeometry->LODIndex < Group.Geometries.Num(), TEXT("FRayTracingGeometry assigned to a group must have a valid LODIndex"));
		checkf(Group.Geometries[InGeometry->LODIndex] == nullptr, TEXT("Each LOD inside a FRayTracingGeometryGroup can only be associated with a single FRayTracingGeometry"));

		Group.Geometries[InGeometry->LODIndex] = InGeometry;
		++Group.NumReferences;

		if (InGeometry->LODIndex >= Group.CurrentFirstLODIdx && IsRayTracingEnabled() && !IsRayTracingUsingReferenceBasedResidency())
		{
			PendingStreamingRequests.Add(Handle);
		}
	}
		
	INC_DWORD_STAT(STAT_RayTracingGeometryCount);

	GRayTracingGeometryManager->RefreshRegisteredGeometry(Handle);

	return Handle;
}

void FRayTracingGeometryManager::ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle Handle)
{
	check(Handle != INDEX_NONE);

	FScopeLock ScopeLock(&MainCS);

	FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Handle];

	if (RegisteredGeometry.Geometry->GroupHandle != INDEX_NONE)
	{
		// if geometry was assigned to a group, clear the relevant entry so another geometry can be registered later

		checkf(RegisteredGroups.IsValidIndex(RegisteredGeometry.Geometry->GroupHandle), TEXT("FRayTracingGeometry.GroupHandle must be valid"));

		FRayTracingGeometryGroup& Group = RegisteredGroups[RegisteredGeometry.Geometry->GroupHandle];

		checkf(RegisteredGeometry.Geometry->LODIndex >= 0 && RegisteredGeometry.Geometry->LODIndex < Group.Geometries.Num(), TEXT("FRayTracingGeometry assigned to a group must have a valid LODIndex"));
		checkf(Group.Geometries[RegisteredGeometry.Geometry->LODIndex] == RegisteredGeometry.Geometry, TEXT("Unexpected mismatch of FRayTracingGeometry in FRayTracingGeometryGroup"));

		Group.Geometries[RegisteredGeometry.Geometry->LODIndex] = nullptr;

		ReleaseRayTracingGeometryGroupReference(RegisteredGeometry.Geometry->GroupHandle);
	}

	int32 NumRemoved = ResidentGeometries.Remove(RegisteredGeometry.Geometry);

	if (NumRemoved > 0)
	{
		TotalResidentSize -= RegisteredGeometry.Size;
	}

	RegisteredGeometries.RemoveAt(Handle);
	ReferencedGeometryHandles.Remove(Handle);
	PendingStreamingRequests.Remove(Handle);

	DEC_DWORD_STAT(STAT_RayTracingGeometryCount);
}

void FRayTracingGeometryManager::SetRayTracingGeometryStreamingData(const FRayTracingGeometry* Geometry, FByteBulkData& BulkData, uint32 Offset, uint32 Size)
{
	FScopeLock ScopeLock(&MainCS);

	checkf(RegisteredGeometries.IsValidIndex(Geometry->RayTracingGeometryHandle), TEXT("SetRayTracingGeometryStreamingData(...) can only be used with FRayTracingGeometry that has been registered with FRayTracingGeometryManager."));

	FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Geometry->RayTracingGeometryHandle];
	RegisteredGeometry.StreamableData = &BulkData;
	RegisteredGeometry.StreamableDataOffset = Offset;
	RegisteredGeometry.StreamableDataSize = Size;
}


void FRayTracingGeometryManager::SetRayTracingGeometryGroupCurrentFirstLODIndex(FRHICommandListBase& RHICmdList, RayTracing::GeometryGroupHandle Handle, uint8 NewCurrentFirstLODIdx)
{
	FScopeLock ScopeLock(&MainCS);

	FRayTracingGeometryGroup& Group = RegisteredGroups[Handle];

	// immediately release streamed out LODs
	if(NewCurrentFirstLODIdx > Group.CurrentFirstLODIdx)
	{
		FRHIResourceReplaceBatcher Batcher(RHICmdList, NewCurrentFirstLODIdx - Group.CurrentFirstLODIdx);
		for (int32 LODIdx = Group.CurrentFirstLODIdx; LODIdx < NewCurrentFirstLODIdx; ++LODIdx)
		{
			// some LODs might be stripped during cook
			// skeletal meshes only create static LOD when rendering as static
			if (Group.Geometries[LODIdx] && !Group.Geometries[LODIdx]->IsEvicted())
			{
				Group.Geometries[LODIdx]->ReleaseRHIForStreaming(Batcher);
			}
		}
	}
	else if(IsRayTracingEnabled() && !IsRayTracingUsingReferenceBasedResidency())
	{
		for (int32 LODIdx = NewCurrentFirstLODIdx; LODIdx < Group.CurrentFirstLODIdx; ++LODIdx)
		{
			if (Group.Geometries[LODIdx])
			{
				PendingStreamingRequests.Add(Group.Geometries[LODIdx]->RayTracingGeometryHandle);
			}
		}
	}

	Group.CurrentFirstLODIdx = NewCurrentFirstLODIdx;
}

void FRayTracingGeometryManager::RefreshRegisteredGeometry(RayTracingGeometryHandle Handle)
{
	FScopeLock ScopeLock(&MainCS);

	if (RegisteredGeometries.IsValidIndex(Handle))
	{
		FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Handle];

		if (RegisteredGeometry.Geometry->IsValid() && !RegisteredGeometry.Geometry->IsEvicted())
		{
			const uint32 OldSize = RegisteredGeometry.Size;
			RegisteredGeometry.Size = RegisteredGeometry.Geometry->GetRHI()->GetSizeInfo().ResultSize;

			bool bAlreadyInSet;
			ResidentGeometries.Add(RegisteredGeometry.Geometry, &bAlreadyInSet);

			if (bAlreadyInSet)
			{
				TotalResidentSize -= OldSize;
			}

			TotalResidentSize += RegisteredGeometry.Size;
		}
		else
		{
			int32 NumRemoved = ResidentGeometries.Remove(RegisteredGeometry.Geometry);

			if (NumRemoved > 0)
			{
				TotalResidentSize -= RegisteredGeometry.Size;
			}
		}

		if (RegisteredGeometry.Geometry->Initializer.Type == ERayTracingGeometryInitializerType::StreamingDestination)
		{
			RegisteredGeometry.Status = FRegisteredGeometry::FStatus::StreamedOut;
		}
	}
}

void FRayTracingGeometryManager::PreRender()
{
	bRenderedFrame = true;
}

void FRayTracingGeometryManager::Tick(FRHICommandList& RHICmdList)
{
	if (IsRunningCommandlet())
	{
		return;
	}

	check(IsInRenderingThread());

	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::Tick);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FRayTracingGeometryManager_Tick);

	// TODO: investigate fine grained locking to minimize blocking progress on render command pipes
	// - Don't touch registered geometry/group arrays from render command pipes
	//   - Separate arrays of free geometry/group handles + HandleAllocationCS
	//   - delay actual registration until PreRender() which happens on Render Thread
	//	 - Tick() doesn't need to lock at all
	// - Refresh requests could be queued and processed during Tick()
	FScopeLock ScopeLock(&MainCS);

#if DO_CHECK
	static uint64 PreviousFrameCounter = GFrameCounterRenderThread - 1;
	checkf(GFrameCounterRenderThread != PreviousFrameCounter, TEXT("FRayTracingGeometryManager::Tick() should only be called once per frame"));
	PreviousFrameCounter = GFrameCounterRenderThread;
#endif

	checkf(IsRayTracingUsingReferenceBasedResidency() || (ReferencedGeometryHandles.IsEmpty() && ReferencedGeometryGroups.IsEmpty()),
		TEXT("ReferencedGeometryHandles and ReferencedGeometryGroups are expected to be empty when not using reference based residency"));

	if (!IsRayTracingEnabled())
	{
		if (bHasRayTracingEnableChanged)
		{
			// evict all geometries
			for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
			{
				if (RegisteredGeometry.Geometry->GetRHI() != nullptr)
				{
					RegisteredGeometry.Geometry->Evict();
				}
			}

			PendingStreamingRequests.Empty();
		}
		else
		{
#if DO_CHECK
			// otherwise just check that everything is evicted
			for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
			{
				checkf(RegisteredGeometry.Geometry->IsEvicted() || RegisteredGeometry.Geometry->GetRHI() == nullptr, TEXT("Ray tracing geometry should be evicted when ray tracing is disabled."));
			}
#endif
		}

		checkf(TotalResidentSize == 0,
			TEXT("TotalResidentSize should be 0 when ray tracing is disabled but is currently %lld.\n")
			TEXT("There's likely some issue tracking resident geometries or not all geometries have been evicted."),
			TotalResidentSize
		);

		check(PendingStreamingRequests.IsEmpty());
	}
	else if (IsRayTracingUsingReferenceBasedResidency())
	{
		check(IsRayTracingEnabled());

		if (!bRenderedFrame)
		{
			ensureMsgf(ReferencedGeometryHandles.IsEmpty() && ReferencedGeometryGroups.IsEmpty(),
				TEXT("Unexpected entries in ReferencedGeometryHandles/ReferencedGeometryGroups. ")
				TEXT("Missing a call to PreRender() or didn't clear the arrays in the last frame?"));
			return;
		}

		bRenderedFrame = false;

		TSet<FRayTracingGeometry*> NotReferencedResidentGeometries = ResidentGeometries;

		TArray<FRayTracingGeometry*> ReferencedGeometries;

		// 1st step
		// - map RayTracingGeometryHandle to FRayTracingGeometry
		//		- we use handles to track referenced geometries because some of the higher level code uses const FRayTracingGeometry*
		for (RayTracingGeometryHandle GeometryHandle : ReferencedGeometryHandles)
		{
			FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];

			ReferencedGeometries.Add(RegisteredGeometry.Geometry);
			NotReferencedResidentGeometries.Remove(RegisteredGeometry.Geometry);
		}

		// 2nd step
		// - add all geometries in referenced groups to ReferencedGeometries
		//		- need to make all geometries in group resident otherwise might not have valid geometry when reducing LOD
		//		- TODO: Could track TargetLOD and only make [TargetLOD ... LastLOD] range resident
		for (RayTracing::GeometryGroupHandle GroupHandle : ReferencedGeometryGroups)
		{
			checkf(RegisteredGroups.IsValidIndex(GroupHandle), TEXT("RayTracingGeometryGroupHandle must be valid"));

			const FRayTracingGeometryGroup& Group = RegisteredGroups[GroupHandle];

			for (uint8 LODIndex = Group.CurrentFirstLODIdx; LODIndex < Group.Geometries.Num(); ++LODIndex)
			{
				FRayTracingGeometry* Geometry = Group.Geometries[LODIndex];

				// some LODs might be stripped during cook
				if (Geometry != nullptr)
				{
					ReferencedGeometries.Add(Geometry);
					NotReferencedResidentGeometries.Remove(Geometry);
				}
			}
		}

#if DO_CHECK
		ensure(ReferencedGeometries.Num() == TSet(ReferencedGeometries).Num());
#endif

		// 3rd step
		// - make referenced geometries resident
		for (FRayTracingGeometry* Geometry : ReferencedGeometries)
		{
			NotReferencedResidentGeometries.Remove(Geometry);

			if (Geometry->IsEvicted())
			{
				Geometry->MakeResident(RHICmdList);
			}

			RequestRayTracingGeometryStreamIn(RHICmdList, Geometry->RayTracingGeometryHandle);
		}

		// 4th step
		// - evict geometries not referenced by TLAS
		// - TODO: keep unreferenced geometries up to some pool size resident to prevent resident/evicted loops
		for (FRayTracingGeometry* Geometry : NotReferencedResidentGeometries)
		{
			if (Geometry->GetRHI() != nullptr)
			{
				Geometry->Evict();
			}
		}
	}
	else
	{
		check(IsRayTracingEnabled());

		if (bHasRayTracingEnableChanged)
		{
			// make all geometries resident
			for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
			{
				if (RegisteredGeometry.Geometry->IsEvicted())
				{
					RegisteredGeometry.Geometry->MakeResident(RHICmdList);
				}

				if (!RequestRayTracingGeometryStreamIn(RHICmdList, RegisteredGeometry.Geometry->RayTracingGeometryHandle))
				{
					PendingStreamingRequests.Add(RegisteredGeometry.Geometry->RayTracingGeometryHandle);
				}
			}
		}
		else
		{
#if DO_CHECK
			// otherwise just check that all geometries are resident
			for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
			{
				checkf(!RegisteredGeometry.Geometry->IsEvicted(), TEXT("Ray tracing geometry should not be evicted when ray tracing is enabled."));
			}
#endif
			
			TSet<RayTracingGeometryHandle> CurrentPendingStreamingRequests;
			Swap(CurrentPendingStreamingRequests, PendingStreamingRequests);
			PendingStreamingRequests.Reserve(CurrentPendingStreamingRequests.Num());

			for (RayTracingGeometryHandle GeometryHandle : CurrentPendingStreamingRequests)
			{
				if (!RequestRayTracingGeometryStreamIn(RHICmdList, GeometryHandle))
				{
					PendingStreamingRequests.Add(GeometryHandle);
				}
			}
		}
	}

	ProcessCompletedStreamingRequests(RHICmdList);

	ReferencedGeometryHandles.Reset();
	ReferencedGeometryGroups.Reset();

	bHasRayTracingEnableChanged = false;

	SET_MEMORY_STAT(STAT_RayTracingGeometryResidentMemory, TotalResidentSize);
}

bool FRayTracingGeometryManager::RequestRayTracingGeometryStreamIn(FRHICommandList& RHICmdList, RayTracingGeometryHandle GeometryHandle)
{
	FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];
	FRayTracingGeometry* Geometry = RegisteredGeometry.Geometry;

	if (Geometry->Initializer.Type != ERayTracingGeometryInitializerType::StreamingDestination
		|| RegisteredGeometry.Status == FRegisteredGeometry::FStatus::Streaming)
	{
		// no streaming required or streaming request already in-flight
		return true;
	}

	if (Geometry->GroupHandle != INDEX_NONE)
	{
		const FRayTracingGeometryGroup& Group = RegisteredGroups[Geometry->GroupHandle];

		if (Geometry->LODIndex < Group.CurrentFirstLODIdx)
		{
			// streaming request no longer necessary
			return true;
		}
	}

	FByteBulkData* StreamableData = RegisteredGeometry.StreamableData;

	TResourceArray<uint8> RawData;
	RawData.SetAllowCPUAccess(true);

	FResourceArrayInterface* OfflineData = nullptr;

	if (RegisteredGeometry.StreamableDataSize == 0)
	{
		// no offline data -> build from VB/IB at runtime

		RegisteredGeometry.Status = FRegisteredGeometry::FStatus::StreamedIn;
	}
	else if (StreamableData->IsBulkDataLoaded())
	{
		{
			const uint8* Ptr = (const uint8*)StreamableData->LockReadOnly();

			FMemoryView MemView(Ptr + RegisteredGeometry.StreamableDataOffset, RegisteredGeometry.StreamableDataSize);

			FMemoryReaderView MemReader(MemView, true);
			RawData.BulkSerialize(MemReader);
			StreamableData->Unlock();
		}

		if (!RawData.IsEmpty())
		{
			OfflineData = &RawData;
		}

		RegisteredGeometry.Status = FRegisteredGeometry::FStatus::StreamedIn;
	}
	else
	{
		checkf(StreamableData->CanLoadFromDisk(), TEXT("Bulk data is not loaded and cannot be loaded from disk!"));
		check(!StreamableData->IsStoredCompressedOnDisk()); // We do not support compressed Bulkdata for this system. Limitation of the streaming request/bulk data

		check(StreamableData->IsUsingIODispatcher());

		if (NumStreamingRequests >= GRayTracingStreamingMaxPendingRequests)
		{
			return false;
		}

		FStreamingRequest& StreamingRequest = StreamingRequests[NextStreamingRequestIndex];
		NextStreamingRequestIndex = (NextStreamingRequestIndex + 1) % GRayTracingStreamingMaxPendingRequests;
		++NumStreamingRequests;

		StreamingRequest.GeometryHandle = Geometry->RayTracingGeometryHandle;
		StreamingRequest.RequestBuffer = FIoBuffer(RegisteredGeometry.StreamableDataSize); // TODO: Use FIoBuffer::Wrap with preallocated memory

		// TODO: We're currently using a single batch per request so we can individually cancel and wait on requests.
		// This isn't ideal and should be revisited in the future.
		FBulkDataBatchRequest::FScatterGatherBuilder Batch = FBulkDataBatchRequest::ScatterGather(1);
		Batch.Read(*StreamableData, RegisteredGeometry.StreamableDataOffset, RegisteredGeometry.StreamableDataSize);
		Batch.Issue(StreamingRequest.RequestBuffer, AIOP_Low, [](FBulkDataRequest::EStatus) {}, StreamingRequest.Request);

		RegisteredGeometry.Status = FRegisteredGeometry::FStatus::Streaming;
	}

	if (RegisteredGeometry.Status == FRegisteredGeometry::FStatus::StreamedIn)
	{
		{
			FRHIResourceReplaceBatcher Batcher(RHICmdList, 1);
			FRayTracingGeometryInitializer IntermediateInitializer = Geometry->Initializer;
			IntermediateInitializer.Type = ERayTracingGeometryInitializerType::StreamingSource;
			IntermediateInitializer.OfflineData = OfflineData;

			FRayTracingGeometryRHIRef IntermediateRayTracingGeometry = RHICmdList.CreateRayTracingGeometry(IntermediateInitializer);

			Geometry->SetRequiresBuild(IntermediateInitializer.OfflineData == nullptr || IntermediateRayTracingGeometry->IsCompressed());

			Geometry->InitRHIForStreaming(IntermediateRayTracingGeometry, Batcher);

			// When Batcher goes out of scope it will add commands to copy the BLAS buffers on RHI thread.
			// We need to do it before we build the current geometry (also on RHI thread).
		}

		Geometry->RequestBuildIfNeeded(RHICmdList, ERTAccelerationStructureBuildPriority::Normal);
	}

	return true;
}

void FRayTracingGeometryManager::ProcessCompletedStreamingRequests(FRHICommandList& RHICmdList)
{
	const int32 StartPendingRequestIndex = (NextStreamingRequestIndex + GRayTracingStreamingMaxPendingRequests - NumStreamingRequests) % GRayTracingStreamingMaxPendingRequests;

	int32 NumCompletedRequests = 0;

	for (int32 Index = 0; Index < NumStreamingRequests; ++Index)
	{
		const int32 PendingRequestIndex = (StartPendingRequestIndex + Index) % GRayTracingStreamingMaxPendingRequests;
		FStreamingRequest& PendingRequest = StreamingRequests[PendingRequestIndex];

		if (PendingRequest.Request.IsCompleted())
		{
			++NumCompletedRequests;

			FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[PendingRequest.GeometryHandle];

			const FRayTracingGeometryGroup& Group = RegisteredGroups[RegisteredGeometry.Geometry->GroupHandle];

			if (RegisteredGeometry.Geometry->IsEvicted() || RegisteredGeometry.Geometry->LODIndex < Group.CurrentFirstLODIdx)
			{
				// skip if geometry was evicted while streaming request was being processed
				continue;
			}

			if (!PendingRequest.Request.IsOk())
			{
				// Retry if IO request failed for some reason

				FByteBulkData* StreamableData = RegisteredGeometry.StreamableData;

				FBulkDataBatchRequest::FScatterGatherBuilder Batch = FBulkDataBatchRequest::ScatterGather(1);
				Batch.Read(*StreamableData, RegisteredGeometry.StreamableDataOffset, RegisteredGeometry.StreamableDataSize);
				Batch.Issue(PendingRequest.RequestBuffer, AIOP_Low, [](FBulkDataRequest::EStatus) {}, PendingRequest.Request);
						
				// TODO: Could other requests already be completed?
				break;
			}
			else
			{
				{
					FMemoryReaderView Ar(PendingRequest.RequestBuffer.GetView(), /*bIsPersistent=*/ true);
					RegisteredGeometry.Geometry->RawData.BulkSerialize(Ar);
				}

				{
					FRHIResourceReplaceBatcher Batcher(RHICmdList, 1);
					FRayTracingGeometryInitializer IntermediateInitializer = RegisteredGeometry.Geometry->Initializer;
					IntermediateInitializer.Type = ERayTracingGeometryInitializerType::StreamingSource;

					if (!RegisteredGeometry.Geometry->RawData.IsEmpty())
					{
						IntermediateInitializer.OfflineData = &RegisteredGeometry.Geometry->RawData;
					}

					FRayTracingGeometryRHIRef IntermediateRayTracingGeometry = RHICmdList.CreateRayTracingGeometry(IntermediateInitializer);

					RegisteredGeometry.Geometry->SetRequiresBuild(IntermediateInitializer.OfflineData == nullptr || IntermediateRayTracingGeometry->IsCompressed());

					RegisteredGeometry.Geometry->InitRHIForStreaming(IntermediateRayTracingGeometry, Batcher);

					// When Batcher goes out of scope it will add commands to copy the BLAS buffers on RHI thread.
					// We need to do it before we build the current geometry (also on RHI thread).
				}

				RegisteredGeometry.Status = FRegisteredGeometry::FStatus::StreamedIn;

				RegisteredGeometry.Geometry->RequestBuildIfNeeded(RHICmdList, ERTAccelerationStructureBuildPriority::Normal);
			}
		}
		else
		{
			// TODO: Could other requests already be completed?
			break;
		}
	}

	NumStreamingRequests -= NumCompletedRequests;
}

void FRayTracingGeometryManager::BoostPriority(BuildRequestIndex InRequestIndex, float InBoostValue)
{
	FScopeLock ScopeLock(&RequestCS);
	GeometryBuildRequests[InRequestIndex].BuildPriority += InBoostValue;
}

void FRayTracingGeometryManager::ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::ForceBuildIfPending);

	FScopeLock ScopeLock(&RequestCS);

	BuildParams.Empty(FMath::Max(BuildParams.Max(), InGeometries.Num()));
	for (const FRayTracingGeometry* Geometry : InGeometries)
	{
		if (Geometry->HasPendingBuildRequest())
		{
			SetupBuildParams(GeometryBuildRequests[Geometry->RayTracingBuildRequestIndex], BuildParams);
		}
	}

	if (BuildParams.Num())
	{
		InCmdList.BuildAccelerationStructures(BuildParams);
	}

	BuildParams.Reset();
}

void FRayTracingGeometryManager::ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::ProcessBuildRequests);

	FScopeLock ScopeLock(&RequestCS);

	if (GeometryBuildRequests.Num() == 0)
	{
		return;
	}

	checkf(BuildParams.IsEmpty(), TEXT("Unexpected entries in BuildParams. The array should've been reset at the end of the previous call."));
	checkf(SortedRequests.IsEmpty(), TEXT("Unexpected entries in SortedRequests. The array should've been reset at the end of the previous call."));

	BuildParams.Empty(FMath::Max(BuildParams.Max(), GeometryBuildRequests.Num()));

	if (GRayTracingMaxBuiltPrimitivesPerFrame <= 0)
	{
		// no limit -> no need to sort

		SortedRequests.Empty(); // free potentially allocated memory

		for (FBuildRequest& Request : GeometryBuildRequests)
		{
			const bool bRemoveFromRequestArray = false; // can't modify array while iterating over it
			SetupBuildParams(Request, BuildParams, bRemoveFromRequestArray);
		}

		// after setting up build params can clear the whole array
		GeometryBuildRequests.Reset();
	}
	else
	{
		SortedRequests.Empty(FMath::Max(SortedRequests.Max(), GeometryBuildRequests.Num()));

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SortRequests);

			// Is there a fast way to extract all entries from sparse array?
			for (const FBuildRequest& Request : GeometryBuildRequests)
			{
				SortedRequests.Add(Request);
			}

			SortedRequests.Sort([](const FBuildRequest& InLHS, const FBuildRequest& InRHS)
				{
					return InLHS.BuildPriority > InRHS.BuildPriority;
				});
		}

		// process n requests each 'frame'
		uint64 PrimitivesBuild = 0;
		bool bAddBuildRequest = true;
		for (FBuildRequest& Request : SortedRequests)
		{
			if (bAddBuildRequest || Request.BuildPriority >= 1.0f) // always build immediate requests
			{
				SetupBuildParams(Request, BuildParams);

				// Requested enough?
				PrimitivesBuild += Request.Owner->Initializer.TotalPrimitiveCount;
				if (!bInBuildAll && (PrimitivesBuild > GRayTracingMaxBuiltPrimitivesPerFrame))
				{
					bAddBuildRequest = false;
				}
			}
			else
			{
				// Increment priority to make sure requests don't starve
				Request.BuildPriority += GRayTracingPendingBuildPriorityBoostPerFrame;
			}
		}

		SortedRequests.Reset();
	}

	// kick actual build request to RHI command list
	InCmdList.BuildAccelerationStructures(BuildParams);

	BuildParams.Reset();
}

void FRayTracingGeometryManager::SetupBuildParams(const FBuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams, bool bRemoveFromRequestArray)
{
	check(InBuildRequest.RequestIndex != INDEX_NONE && InBuildRequest.Owner->RayTracingBuildRequestIndex != INDEX_NONE);
	checkf(InBuildRequest.Owner->GetRHI() != nullptr, TEXT("Build request for FRayTracingGeometry without valid RHI. Was the FRayTracingGeometry evicted or released without calling RemoveBuildRequest()?"));

	FRayTracingGeometryBuildParams BuildParam;
	BuildParam.Geometry = InBuildRequest.Owner->GetRHI();
	BuildParam.BuildMode = InBuildRequest.BuildMode;
	InBuildParams.Add(BuildParam);

	InBuildRequest.Owner->RayTracingBuildRequestIndex = INDEX_NONE;

	if (InBuildRequest.Owner->GroupHandle != INDEX_NONE)
	{
		RequestUpdateCachedRenderState(InBuildRequest.Owner->GroupHandle);
	}

	if (bRemoveFromRequestArray)
	{
		GeometryBuildRequests.RemoveAt(InBuildRequest.RequestIndex);
	}

	DEC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	DEC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, InBuildRequest.Owner->Initializer.TotalPrimitiveCount);
}

void FRayTracingGeometryManager::RegisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	FRayTracingGeometryGroup& Group = RegisteredGroups[InRayTracingGeometryGroupHandle];

	TSet<FPrimitiveSceneProxy*>& ProxiesSet = Group.ProxiesWithCachedRayTracingState;
	check(!ProxiesSet.Contains(Proxy));

	ProxiesSet.Add(Proxy);

	++Group.NumReferences;
}

void FRayTracingGeometryManager::UnregisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	FRayTracingGeometryGroup& Group = RegisteredGroups[InRayTracingGeometryGroupHandle];

	TSet<FPrimitiveSceneProxy*>& ProxiesSet = Group.ProxiesWithCachedRayTracingState;

	verify(ProxiesSet.Remove(Proxy) == 1);

	ReleaseRayTracingGeometryGroupReference(InRayTracingGeometryGroupHandle);
}

void FRayTracingGeometryManager::RequestUpdateCachedRenderState(RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	const TSet<FPrimitiveSceneProxy*>& ProxiesSet = RegisteredGroups[InRayTracingGeometryGroupHandle].ProxiesWithCachedRayTracingState;

	for (FPrimitiveSceneProxy* Proxy : ProxiesSet)
	{
		Proxy->GetScene().UpdateCachedRayTracingState(Proxy);
	}
}

void FRayTracingGeometryManager::AddReferencedGeometry(const FRayTracingGeometry* Geometry)
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());

	if (IsRayTracingUsingReferenceBasedResidency())
	{
		if (RegisteredGeometries.IsValidIndex(Geometry->RayTracingGeometryHandle))
		{
			ReferencedGeometryHandles.Add(Geometry->RayTracingGeometryHandle);
		}
	}
}

void FRayTracingGeometryManager::AddReferencedGeometryGroups(const TSet<RayTracing::GeometryGroupHandle>& GeometryGroups)
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());

	if (IsRayTracingUsingReferenceBasedResidency())
	{
		ReferencedGeometryGroups.Append(GeometryGroups);
	}
	else
	{
		ensureMsgf(GeometryGroups.IsEmpty(), TEXT("Should only track ReferencedGeometryGroups when using using reference based residency"));
	}
}

#if DO_CHECK
bool FRayTracingGeometryManager::IsGeometryReferenced(const FRayTracingGeometry* Geometry) const
{
	return ReferencedGeometryHandles.Contains(Geometry->RayTracingGeometryHandle);
}

bool FRayTracingGeometryManager::IsGeometryGroupReferenced(RayTracing::GeometryGroupHandle GeometryGroup) const
{
	return ReferencedGeometryGroups.Contains(GeometryGroup);
}
#endif

#endif // RHI_RAYTRACING
