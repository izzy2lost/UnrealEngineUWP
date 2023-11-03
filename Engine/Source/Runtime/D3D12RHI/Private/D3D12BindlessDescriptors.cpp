// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12BindlessDescriptors.h"
#include "D3D12Descriptors.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

int32 GBindlessResourceDescriptorHeapSize = 1000 * 1000;
static FAutoConsoleVariableRef CVarBindlessResourceDescriptorHeapSize(
	TEXT("D3D12.Bindless.ResourceDescriptorHeapSize"),
	GBindlessResourceDescriptorHeapSize,
	TEXT("Bindless resource descriptor heap size"),
	ECVF_ReadOnly
);

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
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);
}

FD3D12DescriptorHeap* UE::D3D12BindlessDescriptors::CreateGpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap)
{
	const TCHAR* const HeapName = (InType == ERHIDescriptorHeapType::Standard) ? TEXT("BindlessResources") : TEXT("BindlessSamplers");

	return InDevice->GetDescriptorHeapManager().AllocateIndependentHeap(
		HeapName,
		InType,
		InNewNumDescriptorsPerHeap,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessSamplerManager

FD3D12BindlessSamplerManager::FD3D12BindlessSamplerManager(FD3D12Device* InDevice, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats)
	: FD3D12DeviceChild(InDevice)
	, Allocator(ERHIDescriptorHeapType::Sampler, InNumDescriptors, InStats)
{
	GpuHeap = UE::D3D12BindlessDescriptors::CreateGpuHeap(InDevice, ERHIDescriptorHeapType::Sampler, InNumDescriptors);
}

FRHIDescriptorHandle FD3D12BindlessSamplerManager::Allocate()
{
	FRHIDescriptorHandle Result = Allocator.Allocate();
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

void FD3D12BindlessSamplerManager::UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor)
{
	if (DstHandle.IsValid())
	{
		UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), GpuHeap, DstHandle, SrcDescriptor);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessResourceManager

#if !D3D12RHI_CUSTOM_BINDLESS_RESOURCE_MANAGER

FD3D12BindlessResourceManager::FD3D12BindlessResourceManager(FD3D12Device* InDevice, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats)
	: FD3D12DeviceChild(InDevice)
	, Allocator(ERHIDescriptorHeapType::Standard, InNumDescriptors, InStats)
{
	ConfiguredPipelines = ERHIPipeline::Graphics;
	if (GSupportsEfficientAsyncCompute)
	{
		EnumAddFlags(ConfiguredPipelines, ERHIPipeline::AsyncCompute);
	}

	EnumeratePipelines([&](ERHIPipeline PipelineIndex)
	{
		FPipeline& Pipeline = Pipelines[PipelineIndex];

		Pipeline.CpuHeap = UE::D3D12BindlessDescriptors::CreateCpuHeap(InDevice, ERHIDescriptorHeapType::Standard, InNumDescriptors);
		Pipeline.GpuHeap = UE::D3D12BindlessDescriptors::CreateGpuHeap(InDevice, ERHIDescriptorHeapType::Standard, InNumDescriptors);
	});
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

void FD3D12BindlessResourceManager::UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor)
{
	if (DstHandle.IsValid())
	{
		EnumeratePipelines([&](ERHIPipeline PipelineIndex)
		{
			UE::D3D12Descriptors::CopyDescriptor(GetParentDevice(), Pipelines[PipelineIndex].GpuHeap, DstHandle, SrcDescriptor);
		});
	}
}

void FD3D12BindlessResourceManager::UpdateDescriptor(FRHICommandListBase& RHICmdList, FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor)
{
	UpdateDescriptorImmediately(DstHandle, SrcDescriptor);
}

void FD3D12BindlessResourceManager::FlushPendingDescriptorUpdates(FD3D12CommandContext& Context, ERHIPipeline PipelineIndex, const FD3D12PendingResourceDescriptorUpdates& PendingDescriptorUpdates)
{
	checkNoEntry(); // TODO
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

		ResourceManager = MakeUnique<FD3D12BindlessResourceManager>(GetParentDevice(), NumResourceDescriptors, Stats);
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

		SamplerManager = MakeUnique<FD3D12BindlessSamplerManager>(GetParentDevice(), NumSamplerDescriptors, Stats);
	}
}

FRHIDescriptorHandle FD3D12BindlessDescriptorManager::Allocate(ERHIDescriptorHeapType InType)
{
	if (InType == ERHIDescriptorHeapType::Standard && ResourceManager)
	{
		return ResourceManager->Allocate();
	}

	if (InType == ERHIDescriptorHeapType::Sampler && SamplerManager)
	{
		return SamplerManager->Allocate();
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

void FD3D12BindlessDescriptorManager::UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor)
{
	if (DstHandle.GetType() == ERHIDescriptorHeapType::Standard && ResourceManager)
	{
		ResourceManager->UpdateDescriptorImmediately(DstHandle, SrcDescriptor);
		return;
	}

	if (DstHandle.GetType() == ERHIDescriptorHeapType::Sampler && SamplerManager)
	{
		SamplerManager->UpdateDescriptorImmediately(DstHandle, SrcDescriptor);
		return;
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::UpdateResourceDescriptor(FRHICommandListBase& RHICmdList, FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor)
{
	if (ResourceManager)
	{
		ResourceManager->UpdateDescriptor(RHICmdList, DstHandle, SrcDescriptor);
		return;
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::FlushPendingDescriptorUpdates(FD3D12CommandContext& Context, ERHIPipeline PipelineIndex, const FD3D12PendingResourceDescriptorUpdates& PendingDescriptorUpdates)
{
	if (ResourceManager)
	{
		ResourceManager->FlushPendingDescriptorUpdates(Context, PipelineIndex, PendingDescriptorUpdates);
	}
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

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetResourceHeap(ERHIPipeline Pipeline)
{
	return ResourceManager->GetHeap(Pipeline);
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetSamplerHeap()
{
	return SamplerManager->GetHeap();
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetResourceHeap(ERHIPipeline Pipeline, ERHIBindlessConfiguration InConfiguration)
{
	if (AreResourcesBindless(InConfiguration))
	{
		return GetResourceHeap(Pipeline);
	}

	return nullptr;
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetSamplerHeap(ERHIBindlessConfiguration InConfiguration)
{
	if (AreSamplersBindless(InConfiguration))
	{
		return GetSamplerHeap();
	}

	return nullptr;
}

D3D12_GPU_DESCRIPTOR_HANDLE FD3D12BindlessDescriptorManager::GetResourceGpuHandle(ERHIPipeline Pipeline, FRHIDescriptorHandle InHandle) const
{
	if (ResourceManager && ResourceManager->GetHeap(Pipeline))
	{
		return ResourceManager->GetHeap(Pipeline)->GetGPUSlotHandle(InHandle.GetIndex());
	}
	return D3D12_GPU_DESCRIPTOR_HANDLE{};
}

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
