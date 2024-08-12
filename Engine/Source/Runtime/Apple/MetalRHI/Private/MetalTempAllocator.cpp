// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalTempAllocator.h"
#include "MetalDevice.h"
#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"

FMetalTempAllocator::FMetalTempAllocator(FMetalDevice& InDevice, uint32_t InMinAllocationSize, uint32_t InTargetAllocationLimit)
    : Device(InDevice),
	MinAllocationSize(InMinAllocationSize),
	TargetAllocationLimit(InTargetAllocationLimit)
{
	TotalAllocationStat = GET_STATID(STAT_MetalTempAllocatorAllocatedMemory);
}

FMetalBufferPtr FMetalTempAllocator::Allocate(const uint32_t Size)
{
    FScopeLock lock(&AllocatorLock);
    
	const bool bAppleGPU = Device.GetDevice()->supportsFamily(MTL::GPUFamilyApple1);
		
	FMetalBufferPtr Buffer;
	if(bAppleGPU)
	{
		MTL::SizeAndAlign BufferSizeAndAlign = Device.GetDevice()->heapBufferSizeAndAlign(Size, MTL::ResourceCPUCacheModeWriteCombined | MTL::ResourceHazardTrackingModeUntracked);
		
		uint32 AlignedSize = Align(BufferSizeAndAlign.size, BufferSizeAndAlign.align);
		
		MTLHeapPtr AllocHeap;
		static uint32_t MaxAvailableSize = 0;
		
		for(MTLHeapPtr Heap : Heaps)
		{
			if(Heap->maxAvailableSize(BufferSizeAndAlign.align) >= AlignedSize)
			{
				AllocHeap = Heap;
				break;
			}
		}

		if(AllocHeap.get() == nullptr)
		{
			MTL::HeapDescriptor* Desc = MTL::HeapDescriptor::alloc()->init();
			check(Desc);
			
			uint32_t HeapSize = FMath::Max((uint32_t)MinAllocationSize, AlignedSize);
			
			Desc->setType(MTL::HeapTypeAutomatic);
			Desc->setSize(HeapSize);
			Desc->setStorageMode(MTL::StorageModeShared);
			Desc->setResourceOptions(MTL::CPUCacheModeWriteCombined);
			Desc->setHazardTrackingMode(MTL::HazardTrackingModeUntracked);
			
			AllocHeap = NS::TransferPtr(Device.GetDevice()->newHeap(Desc));
			Desc->release();
			
			TotalAllocated += AllocHeap->size();
			INC_MEMORY_STAT_BY_FName(TotalAllocationStat.GetName(), AllocHeap->size());
			
			Heaps.Add(AllocHeap);
		}
		
		MTLBufferPtr MTLBuffer = NS::TransferPtr(AllocHeap->newBuffer(AlignedSize, MTL::ResourceCPUCacheModeWriteCombined | MTL::ResourceHazardTrackingModeUntracked));
		
		check(MTLBuffer);
		
		Buffer = FMetalBufferPtr(new FMetalBuffer(MTLBuffer));
		Buffer->MarkAllocated();
	}
	else
	{
		Buffer = Device.GetResourceHeap().CreateBuffer(Size, 16, BUF_Volatile, FMetalCommandQueue::GetCompatibleResourceOptions((MTL::ResourceOptions)(BUFFER_CACHE_MODE | MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceStorageModeShared)), true);
	}
	
	check(Buffer);
	
	return Buffer;
}

void FMetalTempAllocator::Cleanup()
{
	FScopeLock lock(&AllocatorLock);
	const bool bAppleGPU = Device.GetDevice()->supportsFamily(MTL::GPUFamilyApple1);
	 
	// Clean up heaps
	if(!bAppleGPU || Heaps.Num() <= 1 || TotalAllocated < TargetAllocationLimit)
	{
		return;
	}
	
	TArray<MTLHeapPtr> ToDestroy;
	
	for(MTLHeapPtr Heap : Heaps)
	{
		if(Heap->usedSize() == 0)
		{
			ToDestroy.Add(Heap);
		}
	}
	
	for(MTLHeapPtr Heap : ToDestroy)
	{
		if(Heaps.Num() <= 1 || TotalAllocated < TargetAllocationLimit)
		{
			break;
		}
		
		TotalAllocated -= Heap->size();
		DEC_MEMORY_STAT_BY_FName(TotalAllocationStat.GetName(), Heap->size());
		
		Heaps.Remove(Heap);
	}
}
