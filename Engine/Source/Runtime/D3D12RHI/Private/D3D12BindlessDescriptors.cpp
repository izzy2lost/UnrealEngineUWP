// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12BindlessDescriptors.h"
#include "D3D12RHIPrivate.h"
#include "D3D12Descriptors.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

int32 GBindlessResourceDescriptorHeapSize = 1000 * 1000;
static FAutoConsoleVariableRef CVarBindlessResourceDescriptorHeapSize(
	TEXT("D3D12.Bindless.ResourceDescriptorHeapSize"),
	GBindlessResourceDescriptorHeapSize,
	TEXT("Bindless resource descriptor heap size"),
	ECVF_ReadOnly
);

static int32 GBindlessResourceDescriptorGarbageCollectLatency = 600;
static FAutoConsoleVariableRef CVarBindlessResourceDescriptorGarbageCollectLatency(
	TEXT("D3D12.Bindless.GarbageCollectLatency"),
	GBindlessResourceDescriptorGarbageCollectLatency,
	TEXT("Amount of update cycles before heap is freed"),
	ECVF_ReadOnly);

int32 GBindlessSamplerDescriptorHeapSize = 2048;
static FAutoConsoleVariableRef CVarBindlessSamplerDescriptorHeapSize(
	TEXT("D3D12.Bindless.SamplerDescriptorHeapSize"),
	GBindlessSamplerDescriptorHeapSize,
	TEXT("Bindless sampler descriptor heap size"),
	ECVF_ReadOnly
);

FD3D12DescriptorHeap* UE::D3D12BindlessDescriptors::CreateCpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap)
{
	const TCHAR* const HeapName = (InType == ERHIDescriptorHeapType::Standard) ? TEXT("BindlessResourcesCPU") : TEXT("BindlessSamplersCPU");

	return InDevice->GetDescriptorHeapManager().AllocateIndependentHeap(
		HeapName,
		InType,
		InNewNumDescriptorsPerHeap,
		ED3D12DescriptorHeapFlags::None
	);
}

FD3D12DescriptorHeap* UE::D3D12BindlessDescriptors::CreateGpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap)
{
	SCOPED_NAMED_EVENT_F(TEXT("CreateNewBindlessHeap (%d)"), FColor::Turquoise, InNewNumDescriptorsPerHeap);

	const TCHAR* const HeapName = (InType == ERHIDescriptorHeapType::Standard) ? TEXT("BindlessResources") : TEXT("BindlessSamplers");

	return InDevice->GetDescriptorHeapManager().AllocateIndependentHeap(
		HeapName,
		InType,
		InNewNumDescriptorsPerHeap,
		ED3D12DescriptorHeapFlags::GpuVisible
	);
}

void UE::D3D12BindlessDescriptors::DeferredFreeHeap(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap)
{
	FD3D12DynamicRHI::GetD3DRHI()->DeferredDelete(InHeap, FD3D12DeferredDeleteObject::EType::BindlessDescriptorHeap);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessSamplerManager

FD3D12BindlessSamplerManager::FD3D12BindlessSamplerManager(FD3D12Device* InDevice, ERHIBindlessConfiguration InConfiguration, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats)
	: FD3D12DeviceChild(InDevice)
	, Allocator(ERHIDescriptorHeapType::Sampler, InNumDescriptors, InStats)
	, Configuration(InConfiguration)
{
	GpuHeap = UE::D3D12BindlessDescriptors::CreateGpuHeap(InDevice, ERHIDescriptorHeapType::Sampler, InNumDescriptors);
}

void FD3D12BindlessSamplerManager::CleanupResources()
{
	GpuHeap = nullptr;
}

FRHIDescriptorHandle FD3D12BindlessSamplerManager::AllocateAndInitialize(FD3D12SamplerState* SamplerState)
{
	FRHIDescriptorHandle Result = Allocator.Allocate();
	if (ensure(Result.IsValid()))
	{
		UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), GpuHeap, Result, SamplerState->OfflineDescriptor);
	}
	check(Result.IsValid());
	return Result;
}

void FD3D12BindlessSamplerManager::Free(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		Allocator.Free(InHandle);
	}
}

void FD3D12BindlessSamplerManager::OpenCommandList(FD3D12CommandContext& Context)
{
	if (GetConfiguration() == ERHIBindlessConfiguration::AllShaders)
	{
		Context.StateCache.GetDescriptorCache()->SetBindlessSamplersHeapDirectly(GetHeap());
	}
}

void FD3D12BindlessSamplerManager::CloseCommandList(FD3D12CommandContext& Context)
{
	if (GetConfiguration() == ERHIBindlessConfiguration::AllShaders)
	{
		Context.StateCache.GetDescriptorCache()->SetBindlessSamplersHeapDirectly(nullptr);
	}
}

FD3D12DescriptorHeap* FD3D12BindlessSamplerManager::GetExplicitHeapForContext(FD3D12CommandContext& Context) const
{
	return GetHeap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessResourceManager

#if !D3D12RHI_CUSTOM_BINDLESS_RESOURCE_MANAGER

FD3D12BindlessResourceManager::FD3D12BindlessResourceManager(FD3D12Device* InDevice, ERHIBindlessConfiguration InConfiguration, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats)
	: FD3D12DeviceChild(InDevice)
	, CpuHeap(UE::D3D12BindlessDescriptors::CreateCpuHeap(InDevice, ERHIDescriptorHeapType::Standard, InNumDescriptors))
	, Allocator(ERHIDescriptorHeapType::Standard, InNumDescriptors, InStats)
	, Configuration(InConfiguration)
{	
	if (GetConfiguration() == ERHIBindlessConfiguration::AllShaders)
	{
		// Always allocate a heap when full bindless
		AddActiveGPUHeap();
	}
}

FRHIDescriptorHandle FD3D12BindlessResourceManager::Allocate()
{
	FRHIDescriptorHandle Result = Allocator.Allocate();
	check(Result.IsValid());
	return Result;
}

void FD3D12BindlessResourceManager::Free(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		Allocator.Free(InHandle);
	}
}

void FD3D12BindlessResourceManager::CleanupResources()
{
	CpuHeap.SafeRelease();

	ReleaseGPUHeaps();
}

void FD3D12BindlessResourceManager::ReleaseGPUHeaps()
{
	if (ActiveGpuHeapIndex >= 0)
	{		
		for (FGpuHeapData& GpuHeap : ActiveGpuHeaps)
		{
			if (GpuHeap.bInUse)
			{
				// Defer delete after GPU is done using it (doesn't want to be recycled anymore)
				GetParentDevice()->GetDescriptorHeapManager().DeferredFreeHeap(GpuHeap.GpuHeap);
			}
			else
			{
				GpuHeap.GpuHeap.SafeRelease();
			}
		}
		ActiveGpuHeaps.Empty();

		for (FGpuHeapData& GpuHeap : PooledGpuHeaps)
		{
			GpuHeap.GpuHeap.SafeRelease();
		}
		PooledGpuHeaps.Empty();

		SET_DWORD_STAT(STAT_D3D12BindlessResourceHeapsInUseByGPU, 0);
		SET_DWORD_STAT(STAT_D3D12BindlessResourceHeapsAllocated, 0);
		SET_DWORD_STAT(STAT_D3D12BindlessResourceHeapsActive, 0);
		SET_MEMORY_STAT(STAT_D3D12BindlessResourceHeapGPUMemoryUsage, 0);

		ActiveGpuHeapIndex = -1;
		InUseGPUHeaps = 0;
	}
}

int FD3D12BindlessResourceManager::AddActiveGPUHeap()
{
	int NewHeapIndex = ActiveGpuHeaps.Num();

	FGpuHeapData& GpuHeapData = ActiveGpuHeaps.AddDefaulted_GetRef();

	// Get GPU heap from pool?			
	if (!PooledGpuHeaps.IsEmpty())
	{
		GpuHeapData = PooledGpuHeaps.Pop(EAllowShrinking::No);
	}
	else
	{
		GpuHeapData.GpuHeap = UE::D3D12BindlessDescriptors::CreateGpuHeap(GetParentDevice(), Allocator.GetType(), Allocator.GetCapacity());

		INC_DWORD_STAT(STAT_D3D12BindlessResourceHeapsAllocated);
		INC_MEMORY_STAT_BY(STAT_D3D12BindlessResourceHeapGPUMemoryUsage, GpuHeapData.GpuHeap->GetMemorySize());
	}

	INC_DWORD_STAT(STAT_D3D12BindlessResourceHeapsActive);

	// Copy over the current CPU state (which contains all updates and latest correct state)
	CopyCpuHeap(GpuHeapData.GpuHeap);

	GpuHeapData.bInUse = true; // mark as in use
	UpdateInUseGPUHeaps(true);

	return NewHeapIndex;
}

void FD3D12BindlessResourceManager::UpdateInUseGPUHeaps(bool bInUse)
{
	if (bInUse)
	{	
		InUseGPUHeaps++;
		MaxInUseGPUHeaps = FMath::Max(MaxInUseGPUHeaps, InUseGPUHeaps);
		INC_DWORD_STAT(STAT_D3D12BindlessResourceHeapsInUseByGPU);
	}
	else
	{
		InUseGPUHeaps--;
		DEC_DWORD_STAT(STAT_D3D12BindlessResourceHeapsInUseByGPU);
	}
}

void FD3D12BindlessResourceManager::GarbageCollect()
{
	FScopeLock ScopeLock(&GpuHeapsCS);
		
	// Release all GPU heaps when bindless heaps have not been used for certain amount of time with bindless for RayTracing only (assume RayTracing disabled)
	if (GetConfiguration() == ERHIBindlessConfiguration::RayTracingShaders && (LastUsedExplicitHeapCycle + GBindlessResourceDescriptorGarbageCollectLatency < GarbageCollectCycle))
	{
		ReleaseGPUHeaps();
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE("FD3D12BindlessResourceManager::GarbageCollect");

		// Update the moving window max gpu heaps and reset the working value
		MovingWindowMaxInUseGPUHeaps.PushValue(MaxInUseGPUHeaps);
		MaxInUseGPUHeaps = InUseGPUHeaps;

		// Check current moving max with extra n heaps for working space - if above set value then release from active heaps to pool
		int32 TargetActiveGPUHeaps = MovingWindowMaxInUseGPUHeaps.GetMax() + 4;
		if (ActiveGpuHeaps.Num() > TargetActiveGPUHeaps)
		{
			for (int32 HeapIndex = 0; HeapIndex < ActiveGpuHeaps.Num(); ++HeapIndex)
			{
				FGpuHeapData& GpuHeap = ActiveGpuHeaps[HeapIndex];
				if (!GpuHeap.bInUse)
				{
					GpuHeap.UpdatedHandles.Empty();
					GpuHeap.LastUsedGarbageCollectCycle = GarbageCollectCycle;

					PooledGpuHeaps.Add(GpuHeap);
					ActiveGpuHeaps.RemoveAtSwap(HeapIndex, EAllowShrinking::No);

					DEC_DWORD_STAT(STAT_D3D12BindlessResourceHeapsActive);

					// Update the active gpu index as well when it was swapped
					if (ActiveGpuHeapIndex == ActiveGpuHeaps.Num())
					{
						ActiveGpuHeapIndex = HeapIndex;
					}

					HeapIndex--;

					// Early out if removed enough
					if (ActiveGpuHeaps.Num() <= TargetActiveGPUHeaps)
					{
						break;
					}
				}
			}
		}

		// Check which pooled heaps might need to be destroyed
		if (GBindlessResourceDescriptorGarbageCollectLatency > 0)
		{
			for (int32 HeapIndex = 0; HeapIndex < PooledGpuHeaps.Num(); ++HeapIndex)
			{
				FGpuHeapData& GpuHeap = PooledGpuHeaps[HeapIndex];
				check(!GpuHeap.bInUse);
				if ((GpuHeap.LastUsedGarbageCollectCycle + GBindlessResourceDescriptorGarbageCollectLatency <= GarbageCollectCycle))
				{
					DEC_DWORD_STAT(STAT_D3D12BindlessResourceHeapsAllocated);
					DEC_MEMORY_STAT_BY(STAT_D3D12BindlessResourceHeapGPUMemoryUsage, GpuHeap.GpuHeap->GetMemorySize());

					GpuHeap.GpuHeap.SafeRelease();
					PooledGpuHeaps.RemoveAtSwap(HeapIndex, EAllowShrinking::No);
					HeapIndex--;
				}
			}
		}
	}

	GarbageCollectCycle++;
}

void FD3D12BindlessResourceManager::Recycle(FD3D12DescriptorHeap* DescriptorHeap)
{
	FScopeLock ScopeLock(&GpuHeapsCS);

	bool bFound = false;
	for (FGpuHeapData& GpuHeap : ActiveGpuHeaps)
	{
		if (GpuHeap.GpuHeap == DescriptorHeap)
		{
			check(GpuHeap.bInUse);
			GpuHeap.bInUse = false;
			bFound = true;

			UpdateInUseGPUHeaps(false);

			break;
		}
	}
	check(bFound);
}

void FD3D12BindlessResourceManager::InitializeDescriptor(FRHIDescriptorHandle DstHandle, FD3D12View* View)
{
	if (DstHandle.IsValid())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE("FD3D12BindlessResourceManager::InitializeDescriptor");

		// Update both CPU and active GPU heap since it's initialization and we know the handle isn't currently in use by the GPU
		FD3D12OfflineDescriptor OfflineCpuHandle = View->GetOfflineCpuHandle();	
		UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), CpuHeap, DstHandle, OfflineCpuHandle);

		// Copy descriptor to active gpu heaps and to dirty list (needs lock because active gpu heap could be changed on RHI thread)
		// (performing ActiveGpuHeapIndex without lock is twice as fast (10 micros instead of 20 in regular FN frame), but not 100% thread safe because it
		//  could be missing an initialization of the copy from cpu has happened before above copy was done).
		FScopeLock ScopeLock(&GpuHeapsCS);
		if (ActiveGpuHeapIndex >= 0)
		{
			UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), ActiveGpuHeaps[ActiveGpuHeapIndex].GpuHeap, DstHandle, OfflineCpuHandle);
			ActiveGpuHeaps[ActiveGpuHeapIndex].UpdatedHandles.Add(DstHandle);
		}

		INC_DWORD_STAT(STAT_D3D12BindlessResourceDescriptorsInitialized);
	}
}

void FD3D12BindlessResourceManager::UpdateDescriptor(FD3D12ContextArray const& Contexts, FRHIDescriptorHandle DstHandle, FD3D12View* View)
{
	if (DstHandle.IsValid())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE("FD3D12BindlessResourceManager::UpdateDescriptor");

		check(IsInRHIThread() || GRHICommandList.Bypass());
	
		// Update the shared CPU heap
		UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), CpuHeap, DstHandle, View->GetOfflineCpuHandle());

		// Add to update list so it's updated for the next heap
		// (see not on lock in InitializeDescriptor)
		FScopeLock ScopeLock(&GpuHeapsCS);
		if (ActiveGpuHeapIndex >= 0)
		{
			// Request allocation of new heap because current GPU heap is used by GPU and can't modify handles in use
			uint32 const GPUIndex = GetParentDevice()->GetGPUIndex();
			for (FD3D12CommandContextBase* ContextBase : Contexts)
			{
				if (ContextBase)
				{
					FD3D12CommandContext& Context = *ContextBase->GetSingleDeviceContext(GPUIndex);
					Context.GetBindlessState().bRequestNewGpuHeap = true;
					check(Context.IsDefaultContext() || !IsRunningRHIInSeparateThread());
				}
			}

			ActiveGpuHeaps[ActiveGpuHeapIndex].UpdatedHandles.Add(DstHandle);
		}

		INC_DWORD_STAT(STAT_D3D12BindlessResourceDescriptorsUpdated);
	}
}

void FD3D12BindlessResourceManager::FlushPendingDescriptorUpdates(FD3D12CommandContext& Context)
{
	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	// Create a new heap because there have been descriptor updates?
	if (State.bRequestNewGpuHeap)
	{
		// First finalize the previous heap if it was set.
		FinalizeHeapOnState(State);

		// Then assign the current heap to the state
		AssignHeapToState(State);

		if (GetConfiguration() == ERHIBindlessConfiguration::AllShaders && ensure(Context.IsOpen()))
		{
			// Finally tell the Context that we're using this heap,
			// this call also makes sure the heap is set on the d3d command list.
			Context.StateCache.GetDescriptorCache()->SwitchToNewBindlessResourceHeap(State.CurrentGpuHeap);
		}
	}
}

void FD3D12BindlessResourceManager::OpenCommandList(FD3D12CommandContext& Context)
{
	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	// Assign the current active Gpu heap to the context
	AssignHeapToState(State);

	if (GetConfiguration() == ERHIBindlessConfiguration::AllShaders)
	{
		// Assign the heap to the descriptor cache
		Context.StateCache.GetDescriptorCache()->SetBindlessResourcesHeapDirectly(State.CurrentGpuHeap);
	}
}

void FD3D12BindlessResourceManager::CloseCommandList(FD3D12CommandContext& Context)
{
	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	// First finalize the current heap if any was set
	FinalizeHeapOnState(State);

	if (GetConfiguration() == ERHIBindlessConfiguration::AllShaders)
	{
		// Then clear the reference from the state cache
		Context.StateCache.GetDescriptorCache()->SetBindlessResourcesHeapDirectly(nullptr);
	}
}

void FD3D12BindlessResourceManager::FinalizeContext(FD3D12CommandContext& Context)
{
	if (Context.IsOpen())
	{
		Context.CloseCommandList();
	}

	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	// If context wasn't opened but did have descriptor updates make sure the shared gpu heap is updated
	// (can happen due to texture reference updates not adding any real GPU work)
	FinalizeHeapOnState(State);

	if (State.UsedHeaps.Num() > 0)
	{
		for (const FD3D12DescriptorHeapPtr& UsedHeap : State.UsedHeaps)
		{
			checkSlow(UsedHeap);

			// Now queue it up for recycle when GPU is done
			UE::D3D12BindlessDescriptors::DeferredFreeHeap(GetParentDevice(), UsedHeap);
		}

		State.UsedHeaps.Empty();
	}

	check(!Context.GetBindlessState().HasAnyPending());
}

FD3D12DescriptorHeap* FD3D12BindlessResourceManager::GetHeap(ERHIPipeline Pipeline) const
{
	checkNoEntry();
	return nullptr;
}

FD3D12DescriptorHeap* FD3D12BindlessResourceManager::GetExplicitHeapForContext(FD3D12CommandContext& Context)
{
	FD3D12ContextBindlessState& State = Context.GetBindlessState();

	// Assign GPU heap when it's still unassigned (can happen when RT only and not been used yet - will get full copy of updated CPU state)
	if (State.CurrentGpuHeap == nullptr && GetConfiguration() == ERHIBindlessConfiguration::RayTracingShaders)
	{
		FScopeLock ScopeLock(&GpuHeapsCS);
		ActiveGpuHeapIndex = AddActiveGPUHeap();
		State.CurrentGpuHeap = ActiveGpuHeaps[ActiveGpuHeapIndex].GpuHeap;
	}

	LastUsedExplicitHeapCycle = GarbageCollectCycle;
	check(State.CurrentGpuHeap);
	return State.CurrentGpuHeap;
}

void FD3D12BindlessResourceManager::CopyCpuHeap(FD3D12DescriptorHeap* DestinationHeap)
{
	// Copy the smallest possible set of descriptors from the CPU heap to the new GPU heap.
	FRHIDescriptorAllocatorRange AllocatedRange(0, 0);
	if (Allocator.GetAllocatedRange(AllocatedRange))
	{
		const uint32 NumDescriptorsToCopy = AllocatedRange.Last - AllocatedRange.First + 1;
		UE::D3D12Descriptors::CopyDescriptors(GetParentDevice(), DestinationHeap, CpuHeap, AllocatedRange.First, NumDescriptorsToCopy);

		INC_DWORD_STAT_BY(STAT_D3D12BindlessResourceGPUDescriptorsCopied, NumDescriptorsToCopy);
	}
}

void FD3D12BindlessResourceManager::AssignHeapToState(FD3D12ContextBindlessState& State)
{
	checkf(State.CurrentGpuHeap == nullptr, TEXT("FinalizeHeapOnState was not called before AssignHeapToState"));

	FScopeLock ScopeLock(&GpuHeapsCS);

	// Do we have a heap allocated, then assign
	if (ActiveGpuHeapIndex >= 0)
	{	
		// By default use the active GPU heap (will be versioned when needed during update while GPU is using it)
		State.CurrentGpuHeap = ActiveGpuHeaps[ActiveGpuHeapIndex].GpuHeap;
	}
	else
	{
		// Should always have a heap when running with full bindless
		check(GetConfiguration() != ERHIBindlessConfiguration::AllShaders);
	}
}

void FD3D12BindlessResourceManager::FinalizeHeapOnState(FD3D12ContextBindlessState& State)
{
	if (State.bRequestNewGpuHeap)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE("FD3D12BindlessResourceManager::FinalizeHeapOnState");

		check(IsInRHIThread() || GRHICommandList.Bypass());

		FScopeLock ScopeLock(&GpuHeapsCS);

		check(ActiveGpuHeaps[ActiveGpuHeapIndex].GpuHeap == State.CurrentGpuHeap || State.CurrentGpuHeap == nullptr);

		// Update the the last used garbage collect cycle before moving over to a new heap
		ActiveGpuHeaps[ActiveGpuHeapIndex].LastUsedGarbageCollectCycle = GarbageCollectCycle;

		// Move the current heap as used previous heaps so it can be garbage collected
		// (Contains valid data upto this point in time)
		State.UsedHeaps.Emplace(ActiveGpuHeaps[ActiveGpuHeapIndex].GpuHeap);

		int32 NumGpuHeaps = ActiveGpuHeaps.Num();

		// Copy over dirty handles to all other heaps so they are updated when reused as well
		for (int32 GpuHeapIndex = 0; GpuHeapIndex < NumGpuHeaps; ++GpuHeapIndex)
		{
			if (GpuHeapIndex != ActiveGpuHeapIndex)
			{
				ActiveGpuHeaps[GpuHeapIndex].UpdatedHandles.Append(ActiveGpuHeaps[ActiveGpuHeapIndex].UpdatedHandles);
			}
		}

		// Try and reuse a pooled heap (incremented from last used to reduce the possible spike on reuse of lots of heap and dirty handle increase)
		int32 NewActiveGpuHeapIndex = -1;
		for (int32 NextIndex = 1; NextIndex < NumGpuHeaps; ++NextIndex)
		{
			int32 GpuHeapIndex = (ActiveGpuHeapIndex + NextIndex) % NumGpuHeaps;

			// Not used by the GPU anymore and not the current one
			if (GpuHeapIndex != ActiveGpuHeapIndex && !ActiveGpuHeaps[GpuHeapIndex].bInUse)
			{
				NewActiveGpuHeapIndex = GpuHeapIndex;
				break;
			}
		}

		// Found a pooled heap, then copy over the dirty descriptor handles
		if (NewActiveGpuHeapIndex >= 0)
		{
			// NOTE: copying over duplicate descriptor entries is faster then adding them to set for reduction
			//		 CitySample there is about 2 to 4 times duplication but still faster to copy all then deduplication

			FGpuHeapData& GpuHeapData = ActiveGpuHeaps[NewActiveGpuHeapIndex];
			INC_DWORD_STAT_BY(STAT_D3D12BindlessResourceGPUDescriptorsCopied, GpuHeapData.UpdatedHandles.Num());

			UE::D3D12Descriptors::CopyDescriptors(GetParentDevice(), GpuHeapData.GpuHeap, CpuHeap, GpuHeapData.UpdatedHandles);
			GpuHeapData.UpdatedHandles.Reset();

			// Mark in use by GPU again
			GpuHeapData.bInUse = true;
			UpdateInUseGPUHeaps(true);
		}
		else
		{
			NewActiveGpuHeapIndex = AddActiveGPUHeap();
		}

		// Update the active gpu index
		ActiveGpuHeapIndex = NewActiveGpuHeapIndex;
		INC_DWORD_STAT(STAT_D3D12BindlessResourceHeapsVersioned);
	}

	// Clear the state data
	State.CurrentGpuHeap = nullptr;
	State.bRequestNewGpuHeap = false;
}

#endif // D3D12RHI_CUSTOM_BINDLESS_RESOURCE_MANAGER

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessDescriptorManager

FD3D12BindlessDescriptorManager::FD3D12BindlessDescriptorManager(FD3D12Device* InDevice)
	: FD3D12DeviceChild(InDevice)
{
}

FD3D12BindlessDescriptorManager::~FD3D12BindlessDescriptorManager() = default;

void FD3D12BindlessDescriptorManager::Init()
{
	ResourcesConfiguration = RHIGetRuntimeBindlessResourcesConfiguration(GMaxRHIShaderPlatform);
	SamplersConfiguration  = RHIGetRuntimeBindlessSamplersConfiguration(GMaxRHIShaderPlatform);

	if (ResourcesConfiguration != ERHIBindlessConfiguration::Disabled)
	{
		const TStatId Stats[] =
		{
			GET_STATID(STAT_ResourceDescriptorsAllocated),
			GET_STATID(STAT_BindlessResourceDescriptorsAllocated),
		};

		uint32 NumResourceDescriptors = GBindlessResourceDescriptorHeapSize;
#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		NumResourceDescriptors += GBindlessOnlineDescriptorHeapBlockSize;
#endif

		ResourceManager = MakeUnique<FD3D12BindlessResourceManager>(GetParentDevice(), ResourcesConfiguration, NumResourceDescriptors, Stats);
	}

	if (SamplersConfiguration != ERHIBindlessConfiguration::Disabled)
	{
		const TStatId Stats[] =
		{
			GET_STATID(STAT_SamplerDescriptorsAllocated),
			GET_STATID(STAT_BindlessSamplerDescriptorsAllocated),
		};

		uint32 NumSamplerDescriptors = GBindlessSamplerDescriptorHeapSize;
		if (NumSamplerDescriptors > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE)
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("D3D12.Bindless.SamplerDescriptorHeapSize was set to %d, which is higher than the D3D12 maximum of %d. Adjusting the value to prevent a crash."),
				NumSamplerDescriptors,
				D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE
			);
			NumSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
		}

		SamplerManager = MakeUnique<FD3D12BindlessSamplerManager>(GetParentDevice(), SamplersConfiguration, NumSamplerDescriptors, Stats);
	}
}

void FD3D12BindlessDescriptorManager::CleanupResources()
{
	if (ResourceManager)
	{
		ResourceManager->CleanupResources();
	}

	if (SamplerManager)
	{
		SamplerManager->CleanupResources();
	}
}

FRHIDescriptorHandle FD3D12BindlessDescriptorManager::AllocateResourceHandle()
{
	if (ResourceManager)
	{
		return ResourceManager->Allocate();
	}

	return FRHIDescriptorHandle();
}

void FD3D12BindlessDescriptorManager::GarbageCollect()
{
	if (ResourceManager)
	{
		ResourceManager->GarbageCollect();
	}
}

void FD3D12BindlessDescriptorManager::Recycle(FD3D12DescriptorHeap* DescriptorHeap)
{
	if (ResourceManager)
	{
		ResourceManager->Recycle(DescriptorHeap);
	}
}

FRHIDescriptorHandle FD3D12BindlessDescriptorManager::AllocateAndInitialize(FD3D12SamplerState* SamplerState)
{
	if (SamplerManager)
	{
		return SamplerManager->AllocateAndInitialize(SamplerState);
	}

	return FRHIDescriptorHandle();
}

void FD3D12BindlessDescriptorManager::ImmediateFree(FRHIDescriptorHandle InHandle)
{
	if (InHandle.GetType() == ERHIDescriptorHeapType::Standard && ResourceManager)
	{
		ResourceManager->Free(InHandle);
		return;
	}

	if (InHandle.GetType() == ERHIDescriptorHeapType::Sampler && SamplerManager)
	{
		SamplerManager->Free(InHandle);
		return;
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::DeferredFreeFromDestructor(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		FD3D12DynamicRHI::GetD3DRHI()->DeferredDelete(InHandle, GetParentDevice());
	}
}

void FD3D12BindlessDescriptorManager::InitializeDescriptor(FRHIDescriptorHandle DstHandle, FD3D12View* View)
{
	if (DstHandle.GetType() == ERHIDescriptorHeapType::Standard && ResourceManager)
	{
		ResourceManager->InitializeDescriptor(DstHandle, View);
		return;
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::UpdateDescriptor(FD3D12ContextArray const& Contexts, FRHIDescriptorHandle DstHandle, FD3D12View* View)
{
	if (ResourceManager)
	{
		ResourceManager->UpdateDescriptor(Contexts, DstHandle, View);
		return;
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::FinalizeContext(FD3D12CommandContext& Context)
{
	if (ResourceManager)
	{
		ResourceManager->FinalizeContext(Context);
	}
}

void FD3D12BindlessDescriptorManager::OpenCommandList(FD3D12CommandContext& Context)
{
	if (ResourceManager)
	{
		ResourceManager->OpenCommandList(Context);
	}

	if (SamplerManager)
	{
		SamplerManager->OpenCommandList(Context);
	}
}

void FD3D12BindlessDescriptorManager::CloseCommandList(FD3D12CommandContext& Context)
{
	if (ResourceManager)
	{
		ResourceManager->CloseCommandList(Context);
	}

	if (SamplerManager)
	{
		SamplerManager->CloseCommandList(Context);
	}
}

void FD3D12BindlessDescriptorManager::FlushPendingDescriptorUpdates(FD3D12CommandContext& Context)
{
	if (ResourceManager)
	{
		ResourceManager->FlushPendingDescriptorUpdates(Context);
	}
}

FD3D12DescriptorHeapPair FD3D12BindlessDescriptorManager::GetExplicitHeapsForContext(FD3D12CommandContext& Context, ERHIBindlessConfiguration InConfiguration)
{
	FD3D12DescriptorHeapPair Result{};

	if (AreResourcesBindless(InConfiguration) && ensure(ResourceManager))
	{
		Result.ResourceHeap = ResourceManager->GetExplicitHeapForContext(Context);
	}

	if (AreSamplersBindless(InConfiguration) && ensure(SamplerManager))
	{
		Result.SamplerHeap = SamplerManager->GetExplicitHeapForContext(Context);
	}

	return Result;
}

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
TRHIPipelineArray<FD3D12DescriptorHeapPtr> FD3D12BindlessDescriptorManager::AllocateResourceHeapsForAllPipelines(int32 InSize)
{
	if (ResourceManager)
	{
		return ResourceManager->AllocateResourceHeapsForAllPipelines(InSize);
	}

	// Bad configuration?
	checkNoEntry();
	return TRHIPipelineArray<FD3D12DescriptorHeapPtr>();
}
#endif // D3D12RHI_USE_CONSTANT_BUFFER_VIEWS

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
