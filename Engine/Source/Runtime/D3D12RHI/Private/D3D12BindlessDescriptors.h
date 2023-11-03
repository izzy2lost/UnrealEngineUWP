// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12RHICommon.h"
#include "RHIDefinitions.h"
#include "RHIDescriptorAllocator.h"
#include "Templates/RefCounting.h"

class FD3D12CommandContext;

struct FD3D12DescriptorHeap;
using FD3D12DescriptorHeapPtr = TRefCountPtr<FD3D12DescriptorHeap>;

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

#include COMPILED_PLATFORM_HEADER(D3D12BindlessDescriptors.h)

namespace UE::D3D12BindlessDescriptors
{
	FD3D12DescriptorHeap* CreateCpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap);
	FD3D12DescriptorHeap* CreateGpuHeap(FD3D12Device* InDevice, ERHIDescriptorHeapType InType, uint32 InNewNumDescriptorsPerHeap);
}

/** Manager specifically for bindless sampler descriptors. */
class FD3D12BindlessSamplerManager : public FD3D12DeviceChild
{
public:
	FD3D12BindlessSamplerManager() = delete;
	FD3D12BindlessSamplerManager(FD3D12Device* InDevice, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats);

	FRHIDescriptorHandle Allocate();
	void                 Free(FRHIDescriptorHandle InHandle);

	void UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor);

	FD3D12DescriptorHeap* GetHeap() { return GpuHeap.GetReference(); }

private:
	FD3D12DescriptorHeapPtr GpuHeap;
	FRHIHeapDescriptorAllocator  Allocator;
};

#if !D3D12RHI_CUSTOM_BINDLESS_RESOURCE_MANAGER

struct FD3D12PendingResourceDescriptorUpdates
{
	TArray<FRHIDescriptorHandle> Handles;
	TArray<D3D12_CPU_DESCRIPTOR_HANDLE> Descriptors;

	int32 Num() const
	{
		return Handles.Num();
	}

	void Add(FRHIDescriptorHandle InHandle, const D3D12_CPU_DESCRIPTOR_HANDLE& InDescriptor)
	{
		if (ensure(InHandle.IsValid()))
		{
			Handles.Emplace(InHandle);
			Descriptors.Emplace(InDescriptor);
		}
	}

	void Empty()
	{
		Handles.Empty();
		Descriptors.Empty();
	}
};

/** Manager specifically for bindless resource descriptors. Has to handle renames on command lists. */
class FD3D12BindlessResourceManager : public FD3D12DeviceChild
{
public:
	FD3D12BindlessResourceManager() = delete;
	FD3D12BindlessResourceManager(FD3D12Device* InDevice, uint32 InNumDescriptors, TConstArrayView<TStatId> InStats);

	FRHIDescriptorHandle Allocate();
	void                 Free(FRHIDescriptorHandle InHandle);

	void UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor);
	void UpdateDescriptor(FRHICommandListBase& RHICmdList, FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor);

	void FlushPendingDescriptorUpdates(FD3D12CommandContext& Context, ERHIPipeline PipelineIndex, const FD3D12PendingResourceDescriptorUpdates& PendingDescriptorUpdates);

	bool IsEnabledForPipeline(ERHIPipeline Pipeline) const
	{
		return EnumHasAnyFlags(ConfiguredPipelines, Pipeline);
	}

	FD3D12DescriptorHeap* GetHeap(ERHIPipeline Pipeline)
	{
		if (IsEnabledForPipeline(Pipeline))
		{
			return Pipelines[Pipeline].GpuHeap.GetReference();
		}
		return nullptr;
	}

private:
	template <typename TFunctionType>
	void EnumeratePipelines(TFunctionType&& Function)
	{
		EnumerateRHIPipelines(ConfiguredPipelines, Forward<TFunctionType>(Function));
	}

	struct FPipeline
	{
		FD3D12DescriptorHeapPtr CpuHeap;
		FD3D12DescriptorHeapPtr GpuHeap;
	};

	ERHIPipeline                 ConfiguredPipelines;
	TRHIPipelineArray<FPipeline> Pipelines;

	FRHIHeapDescriptorAllocator  Allocator;
};

#endif

/** Manager for descriptors used in bindless rendering. */
class FD3D12BindlessDescriptorManager : public FD3D12DeviceChild
{
public:
	FD3D12BindlessDescriptorManager(FD3D12Device* InDevice);
	~FD3D12BindlessDescriptorManager();

	void Init();

	ERHIBindlessConfiguration GetResourcesConfiguration() const { return ResourcesConfiguration; }
	ERHIBindlessConfiguration GetSamplersConfiguration()  const { return SamplersConfiguration; }

	bool AreResourcesBindless() const { return GetResourcesConfiguration() != ERHIBindlessConfiguration::Disabled; }
	bool AreSamplersBindless()  const { return GetSamplersConfiguration()  != ERHIBindlessConfiguration::Disabled; }

	bool AreResourcesBindless(ERHIBindlessConfiguration InConfiguration) const { return GetResourcesConfiguration() != ERHIBindlessConfiguration::Disabled && GetResourcesConfiguration() <= InConfiguration; }
	bool AreSamplersBindless(ERHIBindlessConfiguration InConfiguration)  const { return GetSamplersConfiguration()  != ERHIBindlessConfiguration::Disabled && GetSamplersConfiguration() <= InConfiguration; }

	bool AreResourcesFullyBindless() const { return GetResourcesConfiguration() == ERHIBindlessConfiguration::AllShaders; }
	bool AreSamplersFullyBindless () const { return GetSamplersConfiguration()  == ERHIBindlessConfiguration::AllShaders; }

	FRHIDescriptorHandle Allocate(ERHIDescriptorHeapType InType);
	void                 ImmediateFree(FRHIDescriptorHandle InHandle);
	void                 DeferredFreeFromDestructor(FRHIDescriptorHandle InHandle);

	void UpdateDescriptorImmediately(FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor);
	void UpdateResourceDescriptor(FRHICommandListBase& RHICmdList, FRHIDescriptorHandle DstHandle, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor);

	void FlushPendingDescriptorUpdates(FD3D12CommandContext& Context, ERHIPipeline PipelineIndex, const FD3D12PendingResourceDescriptorUpdates& PendingDescriptorUpdates);

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
	TRHIPipelineArray<FD3D12DescriptorHeapPtr> AllocateResourceHeapsForAllPipelines(int32 InSize);
#endif

	FD3D12DescriptorHeap* GetResourceHeap(ERHIPipeline Pipeline);
	FD3D12DescriptorHeap* GetSamplerHeap();

	FD3D12DescriptorHeap* GetResourceHeap(ERHIPipeline Pipeline, ERHIBindlessConfiguration InConfiguration);
	FD3D12DescriptorHeap* GetSamplerHeap(ERHIBindlessConfiguration InConfiguration);

	D3D12_GPU_DESCRIPTOR_HANDLE GetResourceGpuHandle(ERHIPipeline Pipeline, FRHIDescriptorHandle InHandle) const;

private:
	TUniquePtr<FD3D12BindlessResourceManager> ResourceManager;
	TUniquePtr<FD3D12BindlessSamplerManager>  SamplerManager;

	ERHIBindlessConfiguration ResourcesConfiguration{};
	ERHIBindlessConfiguration SamplersConfiguration{};
};

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
