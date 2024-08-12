// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHI.h"
#include "MetalBuffer.h"
#import "MetalThirdParty.h"
#include "Stats/Stats.h"

class FMetalCommandBuffer;
class FMetalDevice;

/*
   Simple Temporary allocator that allocates from heaps
   Resource lifetime is managed by the code allocating the buffer (recommend Device.ReleaseBuffer)c
 */
class FMetalTempAllocator
{
public:
	FMetalTempAllocator(FMetalDevice& InDevice, uint32_t InMinAllocationSize, uint32_t InTargetAllocationLimit);
	
	FMetalBufferPtr Allocate(const uint32_t Size);
	void Cleanup();
    
private:
	FMetalDevice& Device;
	TArray<MTLHeapPtr> Heaps;
	TArray<FMetalBufferPtr> ActiveBuffers;
	
	FCriticalSection AllocatorLock;
	
	TStatId TotalAllocationStat;
	
	uint32_t TotalAllocated = 0;
	uint32_t MinAllocationSize = 0;
	uint32_t TargetAllocationLimit = 0;
};

