// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/MallocBinnedCommon.h"
#include "Misc/App.h"
#include "Stats/Stats.h"

class FMallocBinnedCommonUtils
{
public:
	template <class AllocType>
	static void TrimThreadFreeBlockLists(AllocType& Allocator, typename AllocType::FPerThreadFreeBlockLists* FreeBlockLists)
	{
		if (FreeBlockLists)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FMallocBinnedCommonUtils::TrimThreadFreeBlockLists);

			for (int32 PoolIndex = 0; PoolIndex != AllocType::NUM_SMALL_POOLS; ++PoolIndex)
			{
				typename AllocType::FBundleNode* Bundles = FreeBlockLists->PopBundles(PoolIndex);
				if (Bundles)
				{
					Allocator.FreeBundles(Bundles, PoolIndex);
				}
			}
		}
	}

	template <class AllocType>
	static void FlushCurrentThreadCache(AllocType& Allocator, bool bNewEpochOnly = false)
	{
		if (typename AllocType::FPerThreadFreeBlockLists* Lists = AllocType::FPerThreadFreeBlockLists::Get())
		{
			if (Lists->UpdateEpoch(Allocator.MemoryTrimEpoch.load(std::memory_order_relaxed)) || !bNewEpochOnly)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FMallocBinnedCommonUtils::FlushCurrentThreadCache);
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FMallocBinnedCommonUtils_FlushCurrentThreadCache);

				const double StartTimeInner = FPlatformTime::Seconds();

				double WaitForMutexTime = 0.0f;
				double WaitForMutexAndTrimTime = 0.0f;

				{
					FScopeLock Lock(&Allocator.GetMutex());
					WaitForMutexTime = FPlatformTime::Seconds() - StartTimeInner;
					TrimThreadFreeBlockLists(Allocator, Lists);
					WaitForMutexAndTrimTime = FPlatformTime::Seconds() - StartTimeInner;
				}

				// These logs must happen outside the above mutex to avoid deadlocks
				if (WaitForMutexTime > GMallocBinnedFlushThreadCacheMaxWaitTime)
				{
					UE_LOG(LogMemory, Warning, TEXT("FMalloc%s took %6.2fms to wait for mutex for trim."), Allocator.GetDescriptiveName(), WaitForMutexTime * 1000.0f);
				}
				if (WaitForMutexAndTrimTime > GMallocBinnedFlushThreadCacheMaxWaitTime)
				{
					UE_LOG(LogMemory, Warning, TEXT("FMalloc%s took %6.2fms to wait for mutex AND trim."), Allocator.GetDescriptiveName(), WaitForMutexAndTrimTime * 1000.0f);
				}
			}
		}
	}

	template <class AllocType>
	static void Trim(AllocType& Allocator)
	{
		// Update the trim epoch so that threads cleanup their thread-local memory when going to sleep.
		Allocator.MemoryTrimEpoch.fetch_add(1, std::memory_order_relaxed);

		QUICK_SCOPE_CYCLE_COUNTER(STAT_FMallocBinnedCommonUtils_Trim);

		// Process thread-local memory caches from as many threads as possible without waking them up.
		// Skip on desktop as we may have too many threads and this could cause some hitches.
		if (!PLATFORM_DESKTOP && GMallocBinnedFlushRegisteredThreadCachesOnOneThread != 0)
		{
			FScopeLock Lock(&Allocator.GetMutex());
			FScopeLock FreeBlockLock(&AllocType::GetFreeBlockListsRegistrationMutex());
			for (typename AllocType::FPerThreadFreeBlockLists* BlockList : AllocType::GetRegisteredFreeBlockLists())
			{
				// If we're unable to lock, it's because the thread is currently active so it
				// will do the flush itself when going back to sleep because we incremented the Epoch.
				if (BlockList->TryLock())
				{
					// Only trim if the epoch has been updated, otherwise the thread already
					// did the trimming when it went to sleep.
					if (BlockList->UpdateEpoch(Allocator.MemoryTrimEpoch.load(std::memory_order_relaxed)))
					{
						TrimThreadFreeBlockLists(Allocator, BlockList);
					}
					BlockList->Unlock();
				}
			}
		}

		TFunction<void(ENamedThreads::Type CurrentThread)> Broadcast =
			[&Allocator](ENamedThreads::Type MyThread)
			{
				// We might already have updated the Epoch so we can skip doing anything costly (i.e. Mutex) in that case.
				const bool bNewEpochOnly = true;
				FlushCurrentThreadCache(Allocator, bNewEpochOnly);
			};

		// Skip task threads on desktop platforms as it is too slow and they don't have much memory
		if (PLATFORM_DESKTOP)
		{
			FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(false, false, Broadcast);
		}
		else
		{
			FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(FPlatformProcess::SupportsMultithreading() && FApp::ShouldUseThreadingForPerformance(), false, Broadcast);
		}
	}
};