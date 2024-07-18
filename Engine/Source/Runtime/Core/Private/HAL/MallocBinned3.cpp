// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/MallocBinned3.h"
#include "HAL/MallocBinnedCommonUtils.h"

PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS

#if PLATFORM_64BITS && PLATFORM_HAS_FPlatformVirtualMemoryBlock
#include "Logging/LogMacros.h"
#include "Misc/ScopeLock.h"
#include "Templates/Function.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "HAL/MemoryMisc.h"
#include "HAL/PlatformMisc.h"

#if UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
#include "HAL/Allocators/CachedOSPageAllocator.h"
#define UE_MB3_MAX_CACHED_OS_FREES (64)
#define UE_MB3_MAX_CACHED_OS_FREES_BYTE_LIMIT (64*1024*1024)

typedef TCachedOSPageAllocator<UE_MB3_MAX_CACHED_OS_FREES, UE_MB3_MAX_CACHED_OS_FREES_BYTE_LIMIT> TBinned3CachedOSPageAllocator;

TBinned3CachedOSPageAllocator& GetCachedOSPageAllocator()
{
	static TBinned3CachedOSPageAllocator Singleton;
	return Singleton;
}

#endif

#if UE_MB3_ALLOW_RUNTIME_TWEAKING
	int32 GBinned3PerThreadCaches = UE_DEFAULT_GBinned3PerThreadCaches;
	static FAutoConsoleVariableRef GMallocBinned3PerThreadCachesCVar(
		TEXT("MallocBinned3.PerThreadCaches"),
		GBinned3PerThreadCaches,
		TEXT("Enables per-thread caches of small (<= 32768 byte) allocations from FMallocBinned3")
		);

	int32 GBinned3MaxBundlesBeforeRecycle = UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle;
	static FAutoConsoleVariableRef GMallocBinned3MaxBundlesBeforeRecycleCVar(
		TEXT("MallocBinned3.BundleRecycleCount"),
		GBinned3MaxBundlesBeforeRecycle,
		TEXT("Number of freed bundles in the global recycler before it returns them to the system, per-block size. Limited by UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle (currently 4)")
		);

	int32 GBinned3AllocExtra = UE_DEFAULT_GBinned3AllocExtra;
	static FAutoConsoleVariableRef GMallocBinned3AllocExtraCVar(
		TEXT("MallocBinned3.AllocExtra"),
		GBinned3AllocExtra,
		TEXT("When we do acquire the lock, how many bins cached in TLS caches. In no case will we grab more than a page.")
		);
#endif

#if UE_MB3_ALLOCATOR_STATS
	int64 Binned3AllocatedSmallPoolMemory = 0; // memory that's requested to be allocated by the game
	int64 Binned3AllocatedOSSmallPoolMemory = 0;

	int64 Binned3AllocatedLargePoolMemory = 0; // memory requests to the OS which don't fit in the small pool
	int64 Binned3AllocatedLargePoolMemoryWAlignment = 0; // when we allocate at OS level we need to align to a size

	TAtomic<int64> Binned3Commits;
	TAtomic<int64> Binned3Decommits;
	int64 Binned3PoolInfoMemory = 0;
	int64 Binned3HashMemory = 0;
	int64 Binned3FreeBitsMemory = 0;
	TAtomic<int64> Binned3TotalPoolSearches;
	TAtomic<int64> Binned3TotalPointerTests;
#endif

#define UE_MB3_TIME_LARGE_BLOCKS (0)

#if UE_MB3_TIME_LARGE_BLOCKS
	TAtomic<double> MemoryRangeReserveTotalTime(0.0);
	TAtomic<int32> MemoryRangeReserveTotalCount(0);

	TAtomic<double> MemoryRangeFreeTotalTime(0.0);
	TAtomic<int32> MemoryRangeFreeTotalCount(0);
#endif

uint16 FMallocBinned3::SmallBinSizesReversedShifted[UE_MB3_SMALL_POOL_COUNT + 1] = { 0 };
uint32 FMallocBinned3::OsAllocationGranularity = 0;

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	uint8* FMallocBinned3::Binned3BaseVMPtr = nullptr;
#else
	uint64 FMallocBinned3::PoolSearchDiv = 0;
	uint8* FMallocBinned3::HighestPoolBaseVMPtr = nullptr;
	uint8* FMallocBinned3::PoolBaseVMPtr[UE_MB3_SMALL_POOL_COUNT] = { nullptr };
#endif

FMallocBinned3* FMallocBinned3::MallocBinned3 = nullptr;
// Mapping of sizes to small table indices
uint8 FMallocBinned3::MemSizeToPoolIndex[1 + (UE_MB3_MAX_SMALL_POOL_SIZE >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT)] = { 0 };

struct FMallocBinned3::FPoolInfoSmall		//This is more like BlockInfoSmall as it stores info per block
{
	enum ECanary
	{
		SmallUnassigned = 0x3,
		SmallAssigned = 0x1
	};

	uint32 Canary : 2;
	uint32 Taken : 15;
	uint32 NoFirstFreeIndex : 1;	// if a small block info has a FirstFreeIndex as it's only 14 bits and can't store UINT_MAX to say there's no first free index
	uint32 FirstFreeIndex : 14;

	FPoolInfoSmall()
		: Canary(ECanary::SmallUnassigned)
		, Taken(0)
		, NoFirstFreeIndex(1)
		, FirstFreeIndex(0)
	{
		static_assert(sizeof(FPoolInfoSmall) == 4, "Padding fail");
	}

	void CheckCanary(ECanary ShouldBe) const
	{
		if (Canary != ShouldBe)
		{
			UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x"), int32(Canary), int32(ShouldBe));
		}
	}

	void SetCanary(ECanary ShouldBe, bool bPreexisting, bool bGuarnteedToBeNew)
	{
		if (bPreexisting)
		{
			if (bGuarnteedToBeNew)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x. This block is both preexisting and guaranteed to be new; which makes no sense."), int32(Canary), int32(ShouldBe));
			}
			if (ShouldBe == ECanary::SmallUnassigned)
			{
				if (Canary != ECanary::SmallAssigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x because this block should be preexisting and in use."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x because this block should be preexisting."), int32(Canary), int32(ShouldBe));
			}
		}
		else
		{
			if (bGuarnteedToBeNew)
			{
				if (Canary != ECanary::SmallUnassigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x. This block is guaranteed to be new yet is it already assigned."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe && Canary != ECanary::SmallUnassigned)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x does not have an expected value."), int32(Canary), int32(ShouldBe));
			}
		}
		Canary = ShouldBe;
	}

	bool HasFreeBin() const
	{
		CheckCanary(ECanary::SmallAssigned);
		return !NoFirstFreeIndex;
	}

	void* AllocateBin(uint8* BlockPtr, uint32 BinSize)
	{
		check(HasFreeBin());
		++Taken;
		check(Taken != 0);
		FFreeBlock* Free = (FFreeBlock*)(BlockPtr + BinSize * FirstFreeIndex);
		void* Result = Free->AllocateBin();
		if (Free->GetNumFreeBins() == 0)
		{
			if (Free->NextFreeBlockIndex == MAX_uint32)
			{
				FirstFreeIndex = 0;
				NoFirstFreeIndex = 1;
			}
			else
			{
				FirstFreeIndex = Free->NextFreeBlockIndex;
				check(uint32(FirstFreeIndex) == Free->NextFreeBlockIndex);
				check(((FFreeBlock*)(BlockPtr + BinSize * FirstFreeIndex))->GetNumFreeBins());
			}
		}

		return Result;
	}
};

struct FMallocBinned3::FPoolInfoLarge
{
	enum class ECanary : uint32
	{
		LargeUnassigned = 0x39431234,
		LargeAssigned = 0x17ea5678,
	};

public:
	ECanary	Canary;
private:
	uint32 AllocSize;						// Number of bytes allocated
	uint32 VMSizeDivVirtualSizeAlignment;	// Number of VM bytes allocated aligned for OS
	uint32 CommitSize;						// Number of bytes committed by the OS

public:
	FPoolInfoLarge() :
		Canary(ECanary::LargeUnassigned),
		AllocSize(0),
		VMSizeDivVirtualSizeAlignment(0),
		CommitSize(0)
	{
	}

	void CheckCanary(ECanary ShouldBe) const
	{
		if (Canary != ShouldBe)
		{
			UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x"), int32(Canary), int32(ShouldBe));
		}
	}

	void SetCanary(ECanary ShouldBe, bool bPreexisting, bool bGuarnteedToBeNew)
	{
		if (bPreexisting)
		{
			if (bGuarnteedToBeNew)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x. This block is both preexisting and guaranteed to be new; which makes no sense."), int32(Canary), int32(ShouldBe));
			}
			if (ShouldBe == ECanary::LargeUnassigned)
			{
				if (Canary != ECanary::LargeAssigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x because this block should be preexisting and in use."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, should be 0x%x because this block should be preexisting."), int32(Canary), int32(ShouldBe));
			}
		}
		else
		{
			if (bGuarnteedToBeNew)
			{
				if (Canary != ECanary::LargeUnassigned)
				{
					UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x. This block is guaranteed to be new yet is it already assigned."), int32(Canary), int32(ShouldBe));
				}
			}
			else if (Canary != ShouldBe && Canary != ECanary::LargeUnassigned)
			{
				UE_LOG(LogMemory, Fatal, TEXT("MallocBinned3 Corruption Canary was 0x%x, will be 0x%x does not have an expected value."), int32(Canary), int32(ShouldBe));
			}
		}
		Canary = ShouldBe;
	}

	uint32 GetOSRequestedBytes() const
	{
		return AllocSize;
	}

	uint32 GetOsCommittedBytes() const
	{
		return CommitSize;
	}

	uint32 GetOsVMPages() const
	{
		CheckCanary(ECanary::LargeAssigned);
		return VMSizeDivVirtualSizeAlignment;
	}

	void SetOSAllocationSize(uint32 InRequestedBytes)
	{
		CheckCanary(ECanary::LargeAssigned);
		AllocSize = InRequestedBytes;
		check(AllocSize > 0 && CommitSize >= AllocSize && VMSizeDivVirtualSizeAlignment * FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment() >= CommitSize);
	}

	void SetOSAllocationSizes(uint32 InRequestedBytes, UPTRINT InCommittedBytes, uint32 InVMSizeDivVirtualSizeAlignment)
	{
		CheckCanary(ECanary::LargeAssigned);
		AllocSize = InRequestedBytes;
		CommitSize = InCommittedBytes;
		VMSizeDivVirtualSizeAlignment = InVMSizeDivVirtualSizeAlignment;
		check(AllocSize > 0 && CommitSize >= AllocSize && VMSizeDivVirtualSizeAlignment * FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment() >= CommitSize);
	}
};

struct FMallocBinned3::Private
{
	// Implementation. 
	static CA_NO_RETURN void OutOfMemory(uint64 Size, uint32 Alignment=0)
	{
		// this is expected not to return
		FPlatformMemory::OnOutOfMemory(Size, Alignment);
	}

	/**
	* Gets the FPoolInfoSmall for a small block memory address. If no valid info exists one is created.
	*/
	static FPoolInfoSmall* GetOrCreatePoolInfoSmall(FMallocBinned3& Allocator, uint32 InPoolIndex, uint32 BlockIndex)
	{
		const uint32 InfosPerPage = Allocator.SmallPoolInfosPerPlatformPage;
		const uint32 InfoOuterIndex = BlockIndex / InfosPerPage;
		const uint32 InfoInnerIndex = BlockIndex % InfosPerPage;
		FPoolInfoSmall*& InfoBlock = Allocator.SmallPoolTables[InPoolIndex].PoolInfos[InfoOuterIndex];
		if (!InfoBlock)
		{
			InfoBlock = (FPoolInfoSmall*)Allocator.AllocateMetaDataMemory(Allocator.OsAllocationGranularity);
#if UE_MB3_ALLOCATOR_STATS
			Binned3PoolInfoMemory += Allocator.OsAllocationGranularity;
#endif
			DefaultConstructItems<FPoolInfoSmall>((void*)InfoBlock, InfosPerPage);
		}

		FPoolInfoSmall* Result = &InfoBlock[InfoInnerIndex];

		bool bGuaranteedToBeNew = false;
		if (BlockIndex >= Allocator.SmallPoolTables[InPoolIndex].NumEverUsedBlocks)
		{
			bGuaranteedToBeNew = true;
			Allocator.SmallPoolTables[InPoolIndex].NumEverUsedBlocks = BlockIndex + 1;
		}
		Result->SetCanary(FPoolInfoSmall::ECanary::SmallAssigned, false, bGuaranteedToBeNew);
		return Result;
	}

	/**
	 * Gets the FPoolInfoLarge for a large block memory address. If no valid info exists one is created.
	 */
	static FPoolInfoLarge* GetOrCreatePoolInfoLarge(FMallocBinned3& Allocator, void* InPtr)
	{
		/** 
		 * Creates an array of FPoolInfo structures for tracking allocations.
		 */
		auto CreatePoolArray = [&Allocator](uint64 NumPools)
		{
			const uint64 PoolArraySize = NumPools * sizeof(FPoolInfoLarge);

			void* Result = Allocator.AllocateMetaDataMemory(PoolArraySize);
#if UE_MB3_ALLOCATOR_STATS
			Binned3PoolInfoMemory += PoolArraySize;
#endif
			if (!Result)
			{
				OutOfMemory(PoolArraySize);
			}

			DefaultConstructItems<FPoolInfoLarge>(Result, NumPools);
			return (FPoolInfoLarge*)Result;
		};

		uint32  BucketIndex;
		UPTRINT BucketIndexCollision;
		uint32  PoolIndex;
		Allocator.PtrToPoolMapping.GetHashBucketAndPoolIndices(InPtr, BucketIndex, BucketIndexCollision, PoolIndex);

		PoolHashBucket* FirstBucket = &Allocator.HashBuckets[BucketIndex];
		PoolHashBucket* Collision   = FirstBucket;
		do
		{
			if (!Collision->FirstPool)
			{
				Collision->BucketIndex = BucketIndexCollision;
				Collision->FirstPool = CreatePoolArray(Allocator.NumLargePoolsPerPage);
				Collision->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, true);
				return &Collision->FirstPool[PoolIndex];
			}

			if (Collision->BucketIndex == BucketIndexCollision)
			{
				Collision->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, false);
				return &Collision->FirstPool[PoolIndex];
			}

			Collision = Collision->Next;
		}
		while (Collision != FirstBucket);

		// Create a new hash bucket entry
		if (!Allocator.HashBucketFreeList)
		{
			{
				Allocator.HashBucketFreeList = (PoolHashBucket*)Allocator.AllocateMetaDataMemory(FMallocBinned3::OsAllocationGranularity);
#if UE_MB3_ALLOCATOR_STATS
				Binned3HashMemory += FMallocBinned3::OsAllocationGranularity;
#endif
			}

			for (UPTRINT i = 0, n = FMallocBinned3::OsAllocationGranularity / sizeof(PoolHashBucket); i < n; ++i)
			{
				Allocator.HashBucketFreeList->Link(new (Allocator.HashBucketFreeList + i) PoolHashBucket());
			}
		}

		PoolHashBucket* NextFree  = Allocator.HashBucketFreeList->Next;
		PoolHashBucket* NewBucket = Allocator.HashBucketFreeList;

		NewBucket->Unlink();

		if (NextFree == NewBucket)
		{
			NextFree = nullptr;
		}
		Allocator.HashBucketFreeList = NextFree;

		if (!NewBucket->FirstPool)
		{
			NewBucket->FirstPool = CreatePoolArray(Allocator.NumLargePoolsPerPage);
			NewBucket->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, true);
		}
		else
		{
			NewBucket->FirstPool[PoolIndex].SetCanary(FPoolInfoLarge::ECanary::LargeAssigned, false, false);
		}

		NewBucket->BucketIndex = BucketIndexCollision;

		FirstBucket->Link(NewBucket);

		return &NewBucket->FirstPool[PoolIndex];
	}

	static FPoolInfoLarge* FindPoolInfo(FMallocBinned3& Allocator, void* InPtr)
	{
		uint32  BucketIndex;
		UPTRINT BucketIndexCollision;
		uint32  PoolIndex;
		Allocator.PtrToPoolMapping.GetHashBucketAndPoolIndices(InPtr, BucketIndex, BucketIndexCollision, PoolIndex);

		PoolHashBucket* FirstBucket = &Allocator.HashBuckets[BucketIndex];
		PoolHashBucket* Collision   = FirstBucket;
		do
		{
			if (Collision->BucketIndex == BucketIndexCollision)
			{
				return &Collision->FirstPool[PoolIndex];
			}

			Collision = Collision->Next;
		}
		while (Collision != FirstBucket);

		return nullptr;
	}

	struct FGlobalRecycler
	{
		bool PushBundle(uint32 InPoolIndex, FBundleNode* InBundle)
		{
			const uint32 NumCachedBundles = FMath::Min<uint32>(GBinned3MaxBundlesBeforeRecycle, UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle);
			for (uint32 Slot = 0; Slot < NumCachedBundles; Slot++)
			{
				if (!Bundles[InPoolIndex].FreeBundles[Slot])
				{
					if (!FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Bundles[InPoolIndex].FreeBundles[Slot], InBundle, nullptr))
					{
						return true;
					}
				}
			}
			return false;
		}

		FBundleNode* PopBundle(uint32 InPoolIndex)
		{
			const uint32 NumCachedBundles = FMath::Min<uint32>(GBinned3MaxBundlesBeforeRecycle, UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle);
			for (uint32 Slot = 0; Slot < NumCachedBundles; Slot++)
			{
				FBundleNode* Result = Bundles[InPoolIndex].FreeBundles[Slot];
				if (Result)
				{
					if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Bundles[InPoolIndex].FreeBundles[Slot], nullptr, Result) == Result)
					{
						return Result;
					}
				}
			}
			return nullptr;
		}

	private:
		struct FPaddedBundlePointer
		{
			FBundleNode* FreeBundles[UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle];
#if (4 + (4 * PLATFORM_64BITS)) * UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle < PLATFORM_CACHE_LINE_SIZE
#	define UE_MB3_BUNDLE_PADDING (PLATFORM_CACHE_LINE_SIZE - sizeof(FBundleNode*) * UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle)
			uint8 Padding[UE_MB3_BUNDLE_PADDING];
#endif
			FPaddedBundlePointer()
			{
				DefaultConstructItems<FBundleNode*>(FreeBundles, UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle);
			}
		};
		static_assert(sizeof(FPaddedBundlePointer) == PLATFORM_CACHE_LINE_SIZE, "FPaddedBundlePointer should be the same size as a cache line");
		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FPaddedBundlePointer Bundles[UE_MB3_SMALL_POOL_COUNT] GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);
	};

	static FGlobalRecycler GGlobalRecycler;

	static void FreeBundles(FMallocBinned3& Allocator, FBundleNode* BundlesToRecycle, uint32 InBinSize, uint32 InPoolIndex)
	{
		FPoolTable& Table = Allocator.SmallPoolTables[InPoolIndex];

		FBundleNode* Bundle = BundlesToRecycle;
		while (Bundle)
		{
			FBundleNode* NextBundle = Bundle->NextBundle;

			FBundleNode* Node = Bundle;
			do
			{
				FBundleNode* NextNode = Node->NextNodeInCurrentBundle;

				uint32 OutBlockIndex;
				void* BaseBlockPtr = Allocator.BlockPointerFromContainedPtr(Node, Allocator.SmallPoolTables[InPoolIndex].NumMemoryPagesPerBlock, OutBlockIndex);
				const uint32 BinIndexWithinBlock = (((uint8*)Node) - ((uint8*)BaseBlockPtr)) / Allocator.SmallPoolTables[InPoolIndex].BinSize;

				FPoolInfoSmall* NodePoolBlock = Allocator.SmallPoolTables[InPoolIndex].PoolInfos[OutBlockIndex / Allocator.SmallPoolInfosPerPlatformPage];
				if (!NodePoolBlock)
				{
					UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to free an unrecognized small block %p"), Node);
				}
				FPoolInfoSmall* NodePool = &NodePoolBlock[OutBlockIndex % Allocator.SmallPoolInfosPerPlatformPage];

				NodePool->CheckCanary(FPoolInfoSmall::ECanary::SmallAssigned);

				const bool bWasExhaused = NodePool->NoFirstFreeIndex;

				// Free a pooled allocation.
				FFreeBlock* Free = (FFreeBlock*)Node;
				Free->NumFreeBins = 1;
				Free->NextFreeBlockIndex = NodePool->NoFirstFreeIndex ? MAX_uint32 : NodePool->FirstFreeIndex;
				Free->BinSizeShifted = (InBinSize >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT);
				Free->Canary = FFreeBlock::CANARY_VALUE;
				Free->PoolIndex = InPoolIndex;
				NodePool->FirstFreeIndex = BinIndexWithinBlock;
				NodePool->NoFirstFreeIndex = 0;
				check(uint32(NodePool->FirstFreeIndex) == BinIndexWithinBlock);

				// Free this pool.
				check(NodePool->Taken >= 1);
				if (--NodePool->Taken == 0)
				{
					NodePool->SetCanary(FPoolInfoSmall::ECanary::SmallUnassigned, true, false);
					Table.BlocksAllocatedBits.FreeBit(OutBlockIndex);

					const uint64 AllocSize = Allocator.SmallPoolTables[InPoolIndex].NumMemoryPagesPerBlock * Allocator.OsAllocationGranularity;

					if (!bWasExhaused)
					{
						Table.BlocksExhaustedBits.AllocBit(OutBlockIndex);
					}

					Allocator.Decommit(InPoolIndex, BaseBlockPtr, AllocSize);
#if UE_MB3_ALLOCATOR_STATS
					Binned3AllocatedOSSmallPoolMemory -= AllocSize;
#endif
				}
				else if (bWasExhaused)
				{
					Table.BlocksExhaustedBits.FreeBit(OutBlockIndex);
				}

				Node = NextNode;
			} while (Node);

			Bundle = NextBundle;
		}
	}

	static FCriticalSection& GetFreeBlockListsRegistrationMutex()
	{
		static FCriticalSection FreeBlockListsRegistrationMutex;
		return FreeBlockListsRegistrationMutex;
	}

	static TArray<FPerThreadFreeBlockLists*>& GetRegisteredFreeBlockLists()
	{
		static TArray<FPerThreadFreeBlockLists*> RegisteredFreeBlockLists;
		return RegisteredFreeBlockLists;
	}

	static void RegisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists)
	{
		FScopeLock Lock(&GetFreeBlockListsRegistrationMutex());
		GetRegisteredFreeBlockLists().Add(FreeBlockLists);
	}

	static void UnregisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists)
	{
		FScopeLock Lock(&GetFreeBlockListsRegistrationMutex());
		GetRegisteredFreeBlockLists().Remove(FreeBlockLists);
#if UE_MB3_ALLOCATOR_STATS
		ConsolidatedMemory.fetch_add(FreeBlockLists->AllocatedMemory, std::memory_order_relaxed);
#endif
	}
};

FMallocBinned3::Private::FGlobalRecycler FMallocBinned3::Private::GGlobalRecycler;

void FMallocBinned3::FreeBundles(FBundleNode* Bundles, uint32 PoolIndex)
{
	Private::FreeBundles(*this, Bundles, PoolIndexToBinSize(PoolIndex), PoolIndex);
}

void FMallocBinned3::RegisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists)
{
	Private::RegisterThreadFreeBlockLists(FreeBlockLists);
}

void FMallocBinned3::UnregisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists)
{
	Private::UnregisterThreadFreeBlockLists(FreeBlockLists);
}

FMallocBinned3::FPoolInfoSmall* FMallocBinned3::PushNewPoolToFront(FMallocBinned3::FPoolTable& Table, uint32 InBinSize, uint32 InPoolIndex, uint32& OutBlockIndex)
{
	const uint32 BlockSize = OsAllocationGranularity * Table.NumMemoryPagesPerBlock;

	// Allocate memory.
	const uint32 BlockIndex = Table.BlocksAllocatedBits.AllocBit();
	if (BlockIndex == MAX_uint32)
	{
		return nullptr;
	}
	uint8* FreePtr = BlockPointerFromIndecies(InPoolIndex, BlockIndex, BlockSize);

	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
	Commit(InPoolIndex, FreePtr, BlockSize);
	const uint64 EndOffset = UPTRINT(FreePtr + BlockSize) - UPTRINT(PoolBasePtr(InPoolIndex));
	if (EndOffset > Table.UnusedAreaOffsetLow)
	{
		Table.UnusedAreaOffsetLow = EndOffset;
	}
	FFreeBlock* Free = new ((void*)FreePtr) FFreeBlock(BlockSize, InBinSize, InPoolIndex);
#if UE_MB3_ALLOCATOR_STATS
	Binned3AllocatedOSSmallPoolMemory += (int64)BlockSize;
#endif
	check(IsAligned(Free, OsAllocationGranularity));
	// Create pool
	FPoolInfoSmall* Result = Private::GetOrCreatePoolInfoSmall(*this, InPoolIndex, BlockIndex);
	Result->CheckCanary(FPoolInfoSmall::ECanary::SmallAssigned);
	Result->Taken = 0;
	Result->FirstFreeIndex = 0;
	Result->NoFirstFreeIndex = 0;
	Table.BlocksExhaustedBits.FreeBit(BlockIndex);

	OutBlockIndex = BlockIndex;

	return Result;
}

FMallocBinned3::FPoolInfoSmall* FMallocBinned3::GetFrontPool(FPoolTable& Table, uint32 InPoolIndex, uint32& OutBlockIndex)
{
	OutBlockIndex = Table.BlocksExhaustedBits.NextAllocBit();
	if (OutBlockIndex == MAX_uint32)
	{
		return nullptr;
	}
	return Private::GetOrCreatePoolInfoSmall(*this, InPoolIndex, OutBlockIndex);
}


FMallocBinned3::FMallocBinned3()
	: HashBucketFreeList(nullptr)
{
	static bool bOnce = false;
	check(!bOnce); // this is now a singleton-like thing and you cannot make multiple copies
	bOnce = true;

	OsAllocationGranularity = FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment();
	checkf(FMath::IsPowerOfTwo(OsAllocationGranularity), TEXT("OS page size must be a power of two"));

	// First thing we try to allocate address space for bins as it might help us to move forward Constants.AddressStart and reduce the amount of available address space for the Large OS Allocs
	// Available address space is used to reserve hash map that can address all of that range, so less addressable space means less memory is allocated for book keeping
#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	Binned3BaseVMBlock = FPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(UE_MB3_SMALL_POOL_COUNT * UE_MB3_MAX_MEMORY_PER_POOL_SIZE, OsAllocationGranularity);
	Binned3BaseVMPtr = (uint8*)Binned3BaseVMBlock.GetVirtualPointer();
	check(IsAligned(Binned3BaseVMPtr, OsAllocationGranularity));
	verify(Binned3BaseVMPtr);
#else

	for (uint32 Index = 0; Index < UE_MB3_SMALL_POOL_COUNT; ++Index)
	{
		FPlatformMemory::FPlatformVirtualMemoryBlock NewBLock = FPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(UE_MB3_MAX_MEMORY_PER_POOL_SIZE, OsAllocationGranularity);

		uint8* NewVM = (uint8*)NewBLock.GetVirtualPointer();
		check(IsAligned(NewVM, OsAllocationGranularity));
		// insertion sort
		if (Index && NewVM < PoolBaseVMPtr[Index - 1])
		{
			uint32 InsertIndex = 0;
			for (; InsertIndex < Index; ++InsertIndex)
			{
				if (NewVM < PoolBaseVMPtr[InsertIndex])
				{
					break;
				}
			}
			check(InsertIndex < Index);
			for (uint32 MoveIndex = Index; MoveIndex > InsertIndex; --MoveIndex)
			{
				PoolBaseVMPtr[MoveIndex] = PoolBaseVMPtr[MoveIndex - 1];
				PoolBaseVMBlock[MoveIndex] = PoolBaseVMBlock[MoveIndex - 1];
			}
			PoolBaseVMPtr[InsertIndex] = NewVM;
			PoolBaseVMBlock[InsertIndex] = NewBLock;
		}
		else
		{
			PoolBaseVMPtr[Index] = NewVM;
			PoolBaseVMBlock[Index] = NewBLock;
		}
	}
	HighestPoolBaseVMPtr = PoolBaseVMPtr[UE_MB3_SMALL_POOL_COUNT - 1];
	uint64 TotalGaps = 0;
	for (uint32 Index = 0; Index < UE_MB3_SMALL_POOL_COUNT - 1; ++Index)
	{
		check(PoolBaseVMPtr[Index + 1] > PoolBaseVMPtr[Index]); // we sorted it
		check(PoolBaseVMPtr[Index + 1] >= PoolBaseVMPtr[Index] + UE_MB3_MAX_MEMORY_PER_POOL_SIZE); // and pools are non-overlapping
		TotalGaps += PoolBaseVMPtr[Index + 1] - (PoolBaseVMPtr[Index] + UE_MB3_MAX_MEMORY_PER_POOL_SIZE);
	}
	if (TotalGaps == 0)
	{
		PoolSearchDiv = 0;
	}
	else if (TotalGaps < UE_MB3_MAX_MEMORY_PER_POOL_SIZE)
	{
		PoolSearchDiv = UE_MB3_MAX_MEMORY_PER_POOL_SIZE; // the gaps are not significant, ignoring them should give accurate searches
	}
	else
	{
		PoolSearchDiv = UE_MB3_MAX_MEMORY_PER_POOL_SIZE + ((TotalGaps + UE_MB3_SMALL_POOL_COUNT - 2) / (UE_MB3_SMALL_POOL_COUNT - 1));
	}
#endif

	FGenericPlatformMemoryConstants Constants = FPlatformMemory::GetConstants();
#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	if (Constants.AddressStart == (uint64)Binned3BaseVMPtr)
	{
		Constants.AddressStart += Align(UE_MB3_SMALL_POOL_COUNT * UE_MB3_MAX_MEMORY_PER_POOL_SIZE, OsAllocationGranularity);
	}
#else
	if (!TotalGaps && Constants.AddressStart == (uint64)PoolBaseVMPtr[0])
	{
		Constants.AddressStart += Align(UE_MB3_SMALL_POOL_COUNT * UE_MB3_MAX_MEMORY_PER_POOL_SIZE, OsAllocationGranularity);
	}
#endif

	// large slab sizes are possible OsAllocationGranularity = 65536;
	NumLargePoolsPerPage = OsAllocationGranularity / sizeof(FPoolInfoLarge);
	check(OsAllocationGranularity % sizeof(FPoolInfoLarge) == 0);  // these need to divide evenly!
	PtrToPoolMapping.Init(OsAllocationGranularity, NumLargePoolsPerPage, Constants.AddressStart, Constants.AddressLimit);

	checkf(Constants.AddressLimit > OsAllocationGranularity, TEXT("OS address limit must be greater than the page size")); // Check to catch 32 bit overflow in AddressLimit
	static_assert(UE_MB3_SMALL_POOL_COUNT <= 256, "Small bins size array size must fit in a byte");
	static_assert(sizeof(FFreeBlock) <= UE_MB3_MINIMUM_ALIGNMENT, "Free block struct must be small enough to fit into the smallest bin");

	// Init pool tables.
	FSizeTableEntry SizeTable[UE_MB3_SMALL_POOL_COUNT];

	verify(FSizeTableEntry::FillSizeTable(OsAllocationGranularity, SizeTable, UE_MB3_BASE_PAGE_SIZE, UE_MB3_MINIMUM_ALIGNMENT, UE_MB3_MAX_SMALL_POOL_SIZE, UE_MB3_BASE_PAGE_SIZE) == UE_MB3_SMALL_POOL_COUNT);
	checkf(SizeTable[UE_MB3_SMALL_POOL_COUNT - 1].BinSize == UE_MB3_MAX_SMALL_POOL_SIZE, TEXT("UE_MB3_MAX_SMALL_POOL_SIZE must be equal to the largest bin size"));
	checkf(sizeof(FMallocBinned3::FFreeBlock) <= SizeTable[0].BinSize, TEXT("Pool header must be able to fit into the smallest bin"));

	SmallPoolInfosPerPlatformPage = OsAllocationGranularity / sizeof(FPoolInfoSmall);

	uint32 RequiredMetaMem = 0;
	for (uint32 Index = 0; Index < UE_MB3_SMALL_POOL_COUNT; ++Index)
	{
		checkf(Index == 0 || SizeTable[Index - 1].BinSize < SizeTable[Index].BinSize, TEXT("Small bin sizes must be strictly increasing"));
		checkf(SizeTable[Index].BinSize % UE_MB3_MINIMUM_ALIGNMENT == 0, TEXT("Small bin size must be a multiple of UE_MB3_MINIMUM_ALIGNMENT"));

		SmallPoolTables[Index].BinSize = SizeTable[Index].BinSize;
		SmallPoolTables[Index].NumMemoryPagesPerBlock = SizeTable[Index].NumMemoryPagesPerBlock;

		SmallPoolTables[Index].UnusedAreaOffsetLow = 0;
		SmallPoolTables[Index].NumEverUsedBlocks = 0;
#if UE_M3_ALLOCATOR_PER_BIN_STATS
		SmallPoolTables[Index].TotalRequestedAllocSize.Store(0);
		SmallPoolTables[Index].TotalAllocCount.Store(0);
		SmallPoolTables[Index].TotalFreeCount.Store(0);
#endif

		const int64 TotalNumberOfBlocks = UE_MB3_MAX_MEMORY_PER_POOL_SIZE / (SizeTable[Index].NumMemoryPagesPerBlock * OsAllocationGranularity);
		const uint32 Size = Align(sizeof(FPoolInfoSmall**) * (TotalNumberOfBlocks + SmallPoolInfosPerPlatformPage - 1) / SmallPoolInfosPerPlatformPage, PLATFORM_CACHE_LINE_SIZE);
		RequiredMetaMem += Size;
#if UE_MB3_ALLOCATOR_STATS
		Binned3PoolInfoMemory += Size;
#endif

		const int64 AllocationSize = Align(FBitTree::GetMemoryRequirements(TotalNumberOfBlocks), PLATFORM_CACHE_LINE_SIZE);
		RequiredMetaMem += AllocationSize * 2;
#if UE_MB3_ALLOCATOR_STATS
		Binned3FreeBitsMemory += AllocationSize * 2;
#endif
	}

	RequiredMetaMem = Align(RequiredMetaMem, OsAllocationGranularity);
	uint8* MetaMem = (uint8*)AllocateMetaDataMemory(RequiredMetaMem);
	const uint8* MetaMemEnd = MetaMem + RequiredMetaMem;
	FMemory::Memzero(MetaMem, RequiredMetaMem);

	for (uint32 Index = 0; Index < UE_MB3_SMALL_POOL_COUNT; ++Index)
	{
		const int64 TotalNumberOfBlocks = UE_MB3_MAX_MEMORY_PER_POOL_SIZE / (SizeTable[Index].NumMemoryPagesPerBlock * OsAllocationGranularity);
		const uint32 Size = Align(sizeof(FPoolInfoSmall**) * (TotalNumberOfBlocks + SmallPoolInfosPerPlatformPage - 1) / SmallPoolInfosPerPlatformPage, PLATFORM_CACHE_LINE_SIZE);

		SmallPoolTables[Index].PoolInfos = (FPoolInfoSmall**)MetaMem;
		MetaMem += Size;

		const int64 AllocationSize = Align(FBitTree::GetMemoryRequirements(TotalNumberOfBlocks), PLATFORM_CACHE_LINE_SIZE);
		SmallPoolTables[Index].BlocksAllocatedBits.FBitTreeInit(TotalNumberOfBlocks, MetaMem, AllocationSize, false);
		MetaMem += AllocationSize;

		SmallPoolTables[Index].BlocksExhaustedBits.FBitTreeInit(TotalNumberOfBlocks, MetaMem, AllocationSize, true);
		MetaMem += AllocationSize;
	}
	check(MetaMem <= MetaMemEnd);

	// Set up pool mappings
	uint8* IndexEntry = MemSizeToPoolIndex;
	uint32 PoolIndex  = 0;
	for (uint32 Index = 0; Index != 1 + (UE_MB3_MAX_SMALL_POOL_SIZE >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT); ++Index)
	{
		const uint32 BinSize = Index << UE_MB3_MINIMUM_ALIGNMENT_SHIFT; // inverse of int32 Index = int32((Size >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT));
		while (SizeTable[PoolIndex].BinSize < BinSize)
		{
			++PoolIndex;
			check(PoolIndex != UE_MB3_SMALL_POOL_COUNT);
		}
		check(PoolIndex < 256);
		*IndexEntry++ = uint8(PoolIndex);
	}

	// now reverse the pool sizes for cache coherency
	for (uint32 Index = 0; Index != UE_MB3_SMALL_POOL_COUNT; ++Index)
	{
		uint32 Partner = UE_MB3_SMALL_POOL_COUNT - Index - 1;
		SmallBinSizesReversedShifted[Index] = (SizeTable[Partner].BinSize >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT);
	}

	uint64 MaxHashBuckets = PtrToPoolMapping.GetMaxHashBuckets();
	{
		int64 HashAllocSize = Align(MaxHashBuckets * sizeof(PoolHashBucket), OsAllocationGranularity);
		HashBuckets = (PoolHashBucket*)AllocateMetaDataMemory(HashAllocSize);
#if UE_MB3_ALLOCATOR_STATS
		Binned3HashMemory += HashAllocSize;
#endif
		verify(HashBuckets);
	}

	DefaultConstructItems<PoolHashBucket>(HashBuckets, MaxHashBuckets);
	MallocBinned3 = this;
	GFixedMallocLocationPtr = (FMalloc**)(&MallocBinned3);
}

FMallocBinned3::~FMallocBinned3()
{
}

void FMallocBinned3::Commit(uint32 InPoolIndex, void *Ptr, SIZE_T Size)
{
#if UE_MB3_ALLOCATOR_STATS
	Binned3Commits++;
#endif

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	Binned3BaseVMBlock.CommitByPtr(Ptr, Size);
#else
	PoolBaseVMBlock[InPoolIndex].CommitByPtr(Ptr, Size);
#endif

	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Ptr, Size));
}

void FMallocBinned3::Decommit(uint32 InPoolIndex, void *Ptr, SIZE_T Size)
{
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));

#if UE_MB3_ALLOCATOR_STATS
	Binned3Decommits++;
#endif

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	Binned3BaseVMBlock.DecommitByPtr(Ptr, Size);
#else
	PoolBaseVMBlock[InPoolIndex].DecommitByPtr(Ptr, Size);
#endif
}

void* FMallocBinned3::AllocateMetaDataMemory(SIZE_T Size)
{
	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);
	const size_t VirtualAlignedSize = Align(Size, FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment());
	FPlatformMemory::FPlatformVirtualMemoryBlock Block = FPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(VirtualAlignedSize);
	const size_t CommitAlignedSize = Align(Size, FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());
	Block.Commit(0, CommitAlignedSize);
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Block.GetVirtualPointer(), CommitAlignedSize));
	return Block.GetVirtualPointer();
}

void FMallocBinned3::FreeMetaDataMemory(void *Ptr, SIZE_T InSize)
{
	if (Ptr)
	{
		LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));

		InSize = Align(InSize, FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment());
		FPlatformMemory::FPlatformVirtualMemoryBlock Block(Ptr, InSize / FPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment());
		Block.FreeVirtual();
	}
}

bool FMallocBinned3::IsInternallyThreadSafe() const
{ 
	return true;
}

void* FMallocBinned3::MallocExternal(SIZE_T Size, uint32 Alignment)
{
	static_assert(DEFAULT_ALIGNMENT <= UE_MB3_MINIMUM_ALIGNMENT, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below

	// Fast path: Allocate from the small pools if the size is small enough and the alignment <= binned3 min alignment.
	//            Larger alignments can waste a lot of memory allocating an entire page, so some smaller alignments are 
	//			  handled in the fallback path if less than a predefined max small pool alignment. 

	bool UsePools = (Size <= UE_MB3_MAX_SMALL_POOL_SIZE) && (Alignment <= UE_MB3_MINIMUM_ALIGNMENT);
	
	if (!UsePools)
	{
		// check if allocations that require alignment larger than UE_MB3_MINIMUM_ALIGNMENT can be promoted to a bin with a natural alignment that matches
		// i.e. 16 bytes allocation with 128 bytes alignment can be promoted to 128 bytes bin
		// this will save us a lot of memory as otherwise allocations will be promoted to OS allocs that are at least 64 KB large, depending on a UE_MB3_MAX_SMALL_POOL_SIZE
		UsePools = PromoteToLargerBin(Size, Alignment, *this);
	}

	if (UsePools) 
	{
		const uint32 PoolIndex = BoundSizeToPoolIndex(Size, MemSizeToPoolIndex);
		FPerThreadFreeBlockLists* Lists = GBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
		if (Lists)
		{
			if (Lists->ObtainRecycledPartial(PoolIndex, Private::GGlobalRecycler))
			{
				if (void* Result = Lists->Malloc(PoolIndex))
				{
#if UE_MB3_ALLOCATOR_STATS
					SmallPoolTables[PoolIndex].HeadEndAlloc(Size);
					const uint32 BinSize = PoolIndexToBinSize(PoolIndex);
					Lists->AllocatedMemory += BinSize;
#endif
					return Result;
				}
			}
		}

		FScopeLock Lock(&Mutex);

		// Allocate from small object pool.
		FPoolTable& Table = SmallPoolTables[PoolIndex];

		uint32 BlockIndex = MAX_uint32;
		FPoolInfoSmall* Pool = GetFrontPool(Table, PoolIndex, BlockIndex);
		if (!Pool)
		{
			Pool = PushNewPoolToFront(Table, Table.BinSize, PoolIndex, BlockIndex);
			
			//Indicates that we run out of Pool memory (512 MB) for this bin type
			if (!Pool)
			{
				if ((PoolIndex + 1) < UE_MB3_SMALL_POOL_COUNT)
				{
					return MallocExternal(SmallPoolTables[PoolIndex + 1].BinSize, Alignment);
				}
				else
				{
					return MallocExternal(UE_MB3_MAX_SMALL_POOL_SIZE + 1, Alignment);
				}
			}
		}

		const uint32 BlockSize = OsAllocationGranularity * Table.NumMemoryPagesPerBlock;
		uint8* BlockPtr = BlockPointerFromIndecies(PoolIndex, BlockIndex, BlockSize);

		void* Result = Pool->AllocateBin(BlockPtr, Table.BinSize);
#if UE_MB3_ALLOCATOR_STATS
		Table.HeadEndAlloc(Size);
		Binned3AllocatedSmallPoolMemory += PoolIndexToBinSize(PoolIndex);
#endif
		if (GBinned3AllocExtra)
		{
			if (Lists)
			{
				// prefill the free list with some allocations so we are less likely to hit this slow path with the mutex 
				for (int32 Index = 0; Index < GBinned3AllocExtra && Pool->HasFreeBin(); Index++)
				{
					if (!Lists->Free(Result, PoolIndex, Table.BinSize))
					{
						break;
					}
					Result = Pool->AllocateBin(BlockPtr, Table.BinSize);
				}
			}
		}
		if (!Pool->HasFreeBin())
		{
			Table.BlocksExhaustedBits.AllocBit(BlockIndex);
		}

		return Result;
	}
	Alignment = FMath::Max<uint32>(Alignment, UE_MB3_MINIMUM_ALIGNMENT);
	Size = Align(FMath::Max((SIZE_T)1, Size), Alignment);

	check(FMath::IsPowerOfTwo(Alignment));

	// Use OS for non-pooled allocations.
	const uint64 AlignedSize = Align(Size, FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());

#if UE_MB3_TIME_LARGE_BLOCKS
	const double StartTime = FPlatformTime::Seconds();
#endif

	LLM_PLATFORM_SCOPE(ELLMTag::FMalloc);

#if UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
	FScopeLock Lock(&Mutex);
	void* Result = GetCachedOSPageAllocator().Allocate(AlignedSize);
	check(IsAligned(Result, Alignment));
#else

	FPlatformMemory::FPlatformVirtualMemoryBlock Block = FPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(AlignedSize, Alignment);
	Block.Commit(0, AlignedSize);
	void* Result = Block.GetVirtualPointer();
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Result, AlignedSize));
#endif

#if UE_MB3_TIME_LARGE_BLOCKS
	const double Add = FPlatformTime::Seconds() - StartTime;
	double Old;
	do
	{
		Old = MemoryRangeReserveTotalTime.Load();
	} while (!MemoryRangeReserveTotalTime.CompareExchange(Old, Old + Add));
	MemoryRangeReserveTotalCount++;
#endif

	UE_CLOG(!IsAligned(Result, Alignment) ,LogMemory, Fatal, TEXT("FMallocBinned3 alignment was too large for OS. Alignment=%d Ptr=%p"), Alignment, Result);

	if (!Result)
	{
		Private::OutOfMemory(AlignedSize);
	}
	check(IsOSAllocation(Result));

#if! UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
	FScopeLock Lock(&Mutex);
#endif

#if UE_MB3_ALLOCATOR_STATS
	Binned3AllocatedLargePoolMemory += Size;
	Binned3AllocatedLargePoolMemoryWAlignment += AlignedSize;
#endif

	// Create pool.
	FPoolInfoLarge* Pool = Private::GetOrCreatePoolInfoLarge(*this, Result);
	check(Size > 0 && Size <= AlignedSize && AlignedSize >= FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());
#if UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
	Pool->SetOSAllocationSizes(Size, AlignedSize, AlignedSize / FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());
#else
	Pool->SetOSAllocationSizes(Size, AlignedSize, Block.GetActualSizeInPages());
#endif

	return Result;
}

void* FMallocBinned3::ReallocExternal(void* Ptr, SIZE_T NewSize, uint32 Alignment)
{
	if (NewSize == 0)
	{
		FMallocBinned3::FreeExternal(Ptr);
		return nullptr;
	}
	static_assert(DEFAULT_ALIGNMENT <= UE_MB3_MINIMUM_ALIGNMENT, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below
	check(FMath::IsPowerOfTwo(Alignment));
	check(Alignment <= OsAllocationGranularity);

	const uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < UE_MB3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		const uint32 BinSize = PoolIndexToBinSize(PoolIndex);
		if (
			((NewSize <= BinSize) & (IsAligned(BinSize, Alignment))) && // one branch, not two
			(PoolIndex == 0 || NewSize > PoolIndexToBinSize(PoolIndex - 1)))
		{
#if UE_MB3_ALLOCATOR_STATS
			SmallPoolTables[PoolIndex].HeadEndAlloc(NewSize);
			SmallPoolTables[PoolIndex].HeadEndFree();
#endif
			return Ptr;
		}

		// Reallocate and copy the data across
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		FMemory::Memcpy(Result, Ptr, FMath::Min<SIZE_T>(NewSize, BinSize));
		FMallocBinned3::FreeExternal(Ptr);
		return Result;
	}
	if (!Ptr)
	{
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		return Result;
	}

	Mutex.Lock();

	// Allocated from OS.
	FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
	if (!Pool)
	{
		UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to realloc an unrecognized pointer %p"), Ptr);
	}
	const uint32 PoolOsBytes = Pool->GetOsCommittedBytes();
	const uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();
	checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::ReallocExternal %u %u"), PoolOSRequestedBytes, PoolOsBytes);
	if (NewSize > PoolOsBytes || // can't fit in the old block
		(NewSize <= UE_MB3_MAX_SMALL_POOL_SIZE && Alignment <= UE_MB3_MINIMUM_ALIGNMENT) || // can switch to the small bin allocator
		Align(NewSize, OsAllocationGranularity) < PoolOsBytes) // we can get some pages back
	{
		Mutex.Unlock();
		// Grow or shrink.
		void* Result = FMallocBinned3::MallocExternal(NewSize, Alignment);
		SIZE_T CopySize = FMath::Min<SIZE_T>(NewSize, PoolOSRequestedBytes);
		FMemory::Memcpy(Result, Ptr, CopySize);
		FMallocBinned3::FreeExternal(Ptr);
		return Result;
	}

#if UE_MB3_ALLOCATOR_STATS
	Binned3AllocatedLargePoolMemory += ((int64)NewSize) - ((int64)PoolOSRequestedBytes);
	// don't need to change the Binned3AllocatedLargePoolMemoryWAlignment because we didn't reallocate so it's the same size
#endif
	
	Pool->SetOSAllocationSize(NewSize);
	Mutex.Unlock();
	return Ptr;
}

void FMallocBinned3::FreeExternal(void* Ptr)
{
	const uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < UE_MB3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		const uint32 BinSize = PoolIndexToBinSize(PoolIndex);

		FBundleNode* BundlesToRecycle = nullptr;
		FPerThreadFreeBlockLists* Lists = GBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
		if (Lists)
		{
			BundlesToRecycle = Lists->RecycleFullBundle(PoolIndex, Private::GGlobalRecycler);
			const bool bPushed = Lists->Free(Ptr, PoolIndex, BinSize);
			check(bPushed);
#if UE_MB3_ALLOCATOR_STATS
			SmallPoolTables[PoolIndex].HeadEndFree();
			Lists->AllocatedMemory -= BinSize;
#endif
		}
		else
		{
			BundlesToRecycle = (FBundleNode*)Ptr;
			BundlesToRecycle->NextNodeInCurrentBundle = nullptr;
		}
		if (BundlesToRecycle)
		{
			BundlesToRecycle->NextBundle = nullptr;
			FScopeLock Lock(&Mutex);
			Private::FreeBundles(*this, BundlesToRecycle, BinSize, PoolIndex);
#if UE_MB3_ALLOCATOR_STATS
			if (!Lists)
			{
				SmallPoolTables[PoolIndex].HeadEndFree();
				// lists track their own stat track them instead in the global stat if we don't have lists
				Binned3AllocatedSmallPoolMemory -= ((int64)(BinSize));
			}
#endif
		}
	}
	else if (Ptr)
	{
#if UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
		FScopeLock Lock(&Mutex);
#endif
		uint32 VMPages;
		{
#if !UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
			FScopeLock Lock(&Mutex);
#endif
			FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
			if (!Pool)
			{
				UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to free an unrecognized pointer %p"), Ptr);
			}
			const uint32 PoolOsBytes = Pool->GetOsCommittedBytes();
			const uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();
			VMPages = Pool->GetOsVMPages();

#if UE_MB3_ALLOCATOR_STATS
			Binned3AllocatedLargePoolMemory -= PoolOSRequestedBytes;
			Binned3AllocatedLargePoolMemoryWAlignment -= PoolOsBytes;
#endif

			checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::FreeExternal %u %u"), int32(PoolOSRequestedBytes), int32(PoolOsBytes));
			Pool->SetCanary(FPoolInfoLarge::ECanary::LargeUnassigned, true, false);
		}

		// Free an OS allocation.
#if UE_MB3_TIME_LARGE_BLOCKS
		const double StartTime = FPlatformTime::Seconds();
#endif
		{
			LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));
#if UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
			GetCachedOSPageAllocator().Free(Ptr, VMPages * FPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment());
#else
			FPlatformMemory::FPlatformVirtualMemoryBlock Block(Ptr, VMPages);
			Block.FreeVirtual();
#endif
		}
#if UE_MB3_TIME_LARGE_BLOCKS
		const double Add = FPlatformTime::Seconds() - StartTime;
		double Old;
		do
		{
			Old = MemoryRangeFreeTotalTime.Load();
		} while (!MemoryRangeFreeTotalTime.CompareExchange(Old, Old + Add));
		MemoryRangeFreeTotalCount++;
#endif
	}
}

bool FMallocBinned3::GetAllocationSizeExternal(void* Ptr, SIZE_T& SizeOut)
{
	const uint64 PoolIndex = PoolIndexFromPtr(Ptr);
	if (PoolIndex < UE_MB3_SMALL_POOL_COUNT)
	{
		check(Ptr); // null is an OS allocation because it will not fall in our VM block
		SizeOut = PoolIndexToBinSize(PoolIndex);
		return true;
	}
	if (!Ptr)
	{
		return false;
	}
	FScopeLock Lock(&Mutex);
	FPoolInfoLarge* Pool = Private::FindPoolInfo(*this, Ptr);
	if (!Pool)
	{
		UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to GetAllocationSizeExternal an unrecognized pointer %p"), Ptr);
	}
	const uint32 PoolOsBytes = Pool->GetOsCommittedBytes();
	const uint32 PoolOSRequestedBytes = Pool->GetOSRequestedBytes();
	checkf(PoolOSRequestedBytes <= PoolOsBytes, TEXT("FMallocBinned3::GetAllocationSizeExternal %u %u"), PoolOSRequestedBytes, PoolOsBytes);
	SizeOut = PoolOsBytes;
	return true;
}

bool FMallocBinned3::ValidateHeap()
{
	// Not implemented
	// NumEverUsedBlocks gives us all of the information we need to examine each pool, so it is doable.
	return true;
}

const TCHAR* FMallocBinned3::GetDescriptiveName()
{
	return TEXT("Binned3");
}

void FMallocBinned3::Trim(bool bTrimThreadCaches)
{
	if (GBinned3PerThreadCaches && bTrimThreadCaches)
	{
		// Trim memory and increase the Epoch.
		FMallocBinnedCommonUtils::Trim(*this);
	}
}

FCriticalSection& FMallocBinned3::GetFreeBlockListsRegistrationMutex()
{
	return Private::GetFreeBlockListsRegistrationMutex();
}

TArray<FMallocBinned3::FPerThreadFreeBlockLists*>& FMallocBinned3::GetRegisteredFreeBlockLists()
{
	return Private::GetRegisteredFreeBlockLists();
}

void FMallocBinned3::SetupTLSCachesOnCurrentThread()
{
	if (!UE_MB3_ALLOW_RUNTIME_TWEAKING && !GBinned3PerThreadCaches)
	{
		return;
	}
	if (!FPlatformTLS::IsValidTlsSlot(FMallocBinned3::BinnedTlsSlot))
	{
		FMallocBinned3::BinnedTlsSlot = FPlatformTLS::AllocTlsSlot();
	}
	check(FPlatformTLS::IsValidTlsSlot(FMallocBinned3::BinnedTlsSlot));
	FPerThreadFreeBlockLists::SetTLS();
}

void FMallocBinned3::ClearAndDisableTLSCachesOnCurrentThread()
{
	if (!UE_MB3_ALLOW_RUNTIME_TWEAKING && !GBinned3PerThreadCaches)
	{
		return;
	}

	FMallocBinnedCommonUtils::FlushCurrentThreadCache(*this);
	FPerThreadFreeBlockLists::ClearTLS();
}

void FMallocBinned3::MarkTLSCachesAsUsedOnCurrentThread()
{
	if (!UE_MB3_ALLOW_RUNTIME_TWEAKING && !GBinned3PerThreadCaches)
	{
		return;
	}

	FPerThreadFreeBlockLists::LockTLS();
}

void FMallocBinned3::MarkTLSCachesAsUnusedOnCurrentThread()
{
	if (!UE_MB3_ALLOW_RUNTIME_TWEAKING && !GBinned3PerThreadCaches)
	{
		return;
	}

	// Will only flush if memory trimming has been called while the thread was active.
	const bool bNewEpochOnly = true;
	FMallocBinnedCommonUtils::FlushCurrentThreadCache(*this, bNewEpochOnly);
	FPerThreadFreeBlockLists::UnlockTLS();
}

void FMallocBinned3::FFreeBlock::CanaryFail() const
{
	UE_LOG(LogMemory, Fatal, TEXT("FMallocBinned3 Attempt to realloc an unrecognized pointer %p   canary == 0x%x != 0x%x"), (void*)this, (int32)Canary, (int32)FMallocBinned3::FFreeBlock::CANARY_VALUE);
}

#if UE_MB3_ALLOCATOR_STATS
int64 FMallocBinned3::GetTotalAllocatedSmallPoolMemory() const
{
	int64 FreeBlockAllocatedMemory = 0;
	{
		FScopeLock Lock(&Private::GetFreeBlockListsRegistrationMutex());
		for (const FPerThreadFreeBlockLists* FreeBlockLists : Private::GetRegisteredFreeBlockLists())
		{
			FreeBlockAllocatedMemory += FreeBlockLists->AllocatedMemory;
		}
		FreeBlockAllocatedMemory += ConsolidatedMemory.load(std::memory_order_relaxed);
	}

	return Binned3AllocatedSmallPoolMemory + FreeBlockAllocatedMemory;
}
#endif

void FMallocBinned3::GetAllocatorStats(FGenericMemoryStats& OutStats)
{
#if UE_MB3_ALLOCATOR_STATS

	const int64 TotalAllocatedSmallPoolMemory = GetTotalAllocatedSmallPoolMemory();

	OutStats.Add(TEXT("Binned3AllocatedSmallPoolMemory"), TotalAllocatedSmallPoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedOSSmallPoolMemory"), Binned3AllocatedOSSmallPoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedLargePoolMemory"), Binned3AllocatedLargePoolMemory);
	OutStats.Add(TEXT("Binned3AllocatedLargePoolMemoryWAlignment"), Binned3AllocatedLargePoolMemoryWAlignment);

	const uint64 TotalAllocated = TotalAllocatedSmallPoolMemory + Binned3AllocatedLargePoolMemory;
	const uint64 TotalOSAllocated = Binned3AllocatedOSSmallPoolMemory + Binned3AllocatedLargePoolMemoryWAlignment;

	OutStats.Add(TEXT("TotalAllocated"), TotalAllocated);
	OutStats.Add(TEXT("TotalOSAllocated"), TotalOSAllocated);
#endif
	FMalloc::GetAllocatorStats(OutStats);
}

#if UE_MB3_ALLOCATOR_STATS && BINNED3_USE_SEPARATE_VM_PER_POOL
void FMallocBinned3::RecordPoolSearch(uint32 Tests) const
{
	Binned3TotalPoolSearches++;
	Binned3TotalPointerTests += Tests;
}
#endif

void FMallocBinned3::DumpAllocatorStats(class FOutputDevice& Ar)
{
#if UE_MB3_ALLOCATOR_STATS
	const int64 TotalAllocatedSmallPoolMemory = GetTotalAllocatedSmallPoolMemory();

	Ar.Logf(TEXT("FMallocBinned3 Mem report"));
	Ar.Logf(TEXT("Constants.BinnedAllocationGranularity = %d"), int32(OsAllocationGranularity));
	Ar.Logf(TEXT("UE_MB3_MAX_SMALL_POOL_SIZE = %d"), int32(UE_MB3_MAX_SMALL_POOL_SIZE));
	Ar.Logf(TEXT("UE_MB3_MAX_MEMORY_PER_POOL_SIZE = %llu"), uint64(UE_MB3_MAX_MEMORY_PER_POOL_SIZE));
	Ar.Logf(TEXT("Small Pool Allocations: %fmb  (including bin size padding)"), ((double)TotalAllocatedSmallPoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Small Pool OS Allocated: %fmb"), ((double)Binned3AllocatedOSSmallPoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Large Pool Requested Allocations: %fmb"), ((double)Binned3AllocatedLargePoolMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Large Pool OS Allocated: %fmb"), ((double)Binned3AllocatedLargePoolMemoryWAlignment) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("PoolInfo: %fmb"), ((double)Binned3PoolInfoMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Hash: %fmb"), ((double)Binned3HashMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Free Bits: %fmb"), ((double)Binned3FreeBitsMemory) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("TLS: %fmb"), ((double)TLSMemory.load(std::memory_order_relaxed)) / (1024.0f * 1024.0f));
	Ar.Logf(TEXT("Slab Commits: %llu"), Binned3Commits.Load());
	Ar.Logf(TEXT("Slab Decommits: %llu"), Binned3Decommits.Load());
#if BINNED3_USE_SEPARATE_VM_PER_POOL
	Ar.Logf(TEXT("BINNED3_USE_SEPARATE_VM_PER_POOL is true - VM is Contiguous = %d"), PoolSearchDiv == 0);
	if (PoolSearchDiv)
	{
		Ar.Logf(TEXT("%llu Pointer Searches   %llu Pointer Compares    %llu Compares/Search"), Binned3TotalPoolSearches.Load(), Binned3TotalPointerTests.Load(), Binned3TotalPointerTests.Load() / Binned3TotalPoolSearches.Load());
		const uint64 TotalMem = PoolBaseVMPtr[UE_MB3_SMALL_POOL_COUNT - 1] + UE_MB3_MAX_MEMORY_PER_POOL_SIZE - PoolBaseVMPtr[0];
		const uint64 MinimumMem = uint64(UE_MB3_SMALL_POOL_COUNT) * UE_MB3_MAX_MEMORY_PER_POOL_SIZE;
		Ar.Logf(TEXT("Percent of gaps in the address range %6.4f  (hopefully < 1, or the searches above will suffer)"), 100.0f * (1.0f - float(MinimumMem) / float(TotalMem)));
	}
#else
	Ar.Logf(TEXT("BINNED3_USE_SEPARATE_VM_PER_POOL is false"));
#endif
	Ar.Logf(TEXT("Total allocated from OS: %fmb"), 
		((double)
			Binned3AllocatedOSSmallPoolMemory + Binned3AllocatedLargePoolMemoryWAlignment + Binned3PoolInfoMemory + Binned3HashMemory + Binned3FreeBitsMemory + TLSMemory.load(std::memory_order_relaxed)
			) / (1024.0f * 1024.0f));


#if UE_MB3_TIME_LARGE_BLOCKS
	Ar.Logf(TEXT("MemoryRangeReserve %d calls %6.3fs    %6.3fus / call"), MemoryRangeReserveTotalCount.Load(), float(MemoryRangeReserveTotalTime.Load()), float(MemoryRangeReserveTotalTime.Load()) * 1000000.0f / float(MemoryRangeReserveTotalCount.Load()));
	Ar.Logf(TEXT("MemoryRangeFree    %d calls %6.3fs    %6.3fus / call"), MemoryRangeFreeTotalCount.Load(), float(MemoryRangeFreeTotalTime.Load()), float(MemoryRangeFreeTotalTime.Load()) * 1000000.0f / float(MemoryRangeFreeTotalCount.Load()));
#endif

#if UE_M3_ALLOCATOR_PER_BIN_STATS
	for (int32 PoolIndex = 0; PoolIndex < UE_MB3_SMALL_POOL_COUNT; PoolIndex++)
	{
		const int64 VM = SmallPoolTables[PoolIndex].UnusedAreaOffsetLow;
		const uint32 CommittedBlocks = SmallPoolTables[PoolIndex].BlocksAllocatedBits.CountOnes(SmallPoolTables[PoolIndex].NumEverUsedBlocks);
		const uint32 PartialBlocks = SmallPoolTables[PoolIndex].NumEverUsedBlocks - SmallPoolTables[PoolIndex].BlocksExhaustedBits.CountOnes(SmallPoolTables[PoolIndex].NumEverUsedBlocks);
		const uint32 FullBlocks = CommittedBlocks - PartialBlocks;
		const int64 ComittedVM = VM - (SmallPoolTables[PoolIndex].NumEverUsedBlocks - CommittedBlocks) * SmallPoolTables[PoolIndex].NumMemoryPagesPerBlock * OsAllocationGranularity;

		const int64 AveSize = SmallPoolTables[PoolIndex].TotalAllocCount.Load() ? SmallPoolTables[PoolIndex].TotalRequestedAllocSize.Load() / SmallPoolTables[PoolIndex].TotalAllocCount.Load() : 0;
		const int64 EstPadWaste = ((SmallPoolTables[PoolIndex].TotalAllocCount.Load() - SmallPoolTables[PoolIndex].TotalFreeCount.Load()) * (PoolIndexToBinSize(PoolIndex) - AveSize));

		Ar.Logf(TEXT("Pool %2d   Size %6d   Allocs %8lld  Frees %8lld  AveAllocSize %6d  EstPadWaste %4dKB  UsedVM %3dMB  CommittedVM %3dMB  HighSlabs %6d  CommittedSlabs %6d  FullSlabs %6d  PartialSlabs  %6d"), 
			PoolIndex,
			PoolIndexToBinSize(PoolIndex),
			SmallPoolTables[PoolIndex].TotalAllocCount.Load(),
			SmallPoolTables[PoolIndex].TotalFreeCount.Load(),
			AveSize,
			EstPadWaste / 1024,
			VM / (1024 * 1024),
			ComittedVM / (1024 * 1024),
			SmallPoolTables[PoolIndex].NumEverUsedBlocks,
			CommittedBlocks,
			FullBlocks,
			PartialBlocks
			);
	}
#endif

#else
	Ar.Logf(TEXT("Allocator Stats for Binned3 are not in this build set UE_MB3_ALLOCATOR_STATS 1 in MallocBinned3.cpp"));
#endif
}
#if !UE_MB3_INLINE
	#if PLATFORM_USES_FIXED_GMalloc_CLASS && !FORCE_ANSI_ALLOCATOR && USE_MALLOC_BINNED3
		//#define FMEMORY_INLINE_FUNCTION_DECORATOR  FORCEINLINE
		#define FMEMORY_INLINE_GMalloc (FMallocBinned3::MallocBinned3)
		#include "FMemory.inl"
	#endif
#endif
#endif

PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS
