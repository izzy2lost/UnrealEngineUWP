// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if PLATFORM_64BITS && PLATFORM_HAS_FPlatformVirtualMemoryBlock
#include "HAL/Allocators/CachedOSPageAllocator.h"
#include "HAL/Allocators/PooledVirtualMemoryAllocator.h"
#include "HAL/CriticalSection.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/MallocBinnedCommon.h"
#include "HAL/PlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "Math/NumericLimits.h"
#include "Misc/AssertionMacros.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/Atomic.h"

struct FGenericMemoryStats;

#define UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS (0)

#ifndef USE_512MB_MAX_MEMORY_PER_BLOCK_SIZE 
#	define USE_512MB_MAX_MEMORY_PER_BLOCK_SIZE		0
#endif

#define UE_MB3_BASE_PAGE_SIZE						4096			// Minimum "page size" for binned3
#define UE_MB3_MINIMUM_ALIGNMENT_SHIFT				4				// Alignment of bins, expressed as a shift
#define UE_MB3_MINIMUM_ALIGNMENT					16				// Alignment of bins

#ifndef BINNED3_MAX_SMALL_POOL_ALIGNMENT
#	define BINNED3_MAX_SMALL_POOL_ALIGNMENT			128
#endif


#ifndef UE_MB3_MAX_SMALL_POOL_SIZE
#	if UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS
#		define UE_MB3_MAX_SMALL_POOL_SIZE			(UE_MBC_MAX_LISTED_SMALL_POOL_SIZE)	// Maximum small bin size
#	else
#		define UE_MB3_MAX_SMALL_POOL_SIZE			(128 * 1024)	// Maximum small bin size
#	endif
#endif
#define UE_MB3_SMALL_POOL_COUNT						(UE_MBC_NUM_LISTED_SMALL_POOLS + (UE_MB3_MAX_SMALL_POOL_SIZE - UE_MBC_MAX_LISTED_SMALL_POOL_SIZE) / UE_MB3_BASE_PAGE_SIZE)

#if USE_512MB_MAX_MEMORY_PER_BLOCK_SIZE
#	define UE_MB3_MAX_MEMORY_PER_POOL_SIZE_SHIFT	(29) // maximum of 512MB per pool
#else
#	define UE_MB3_MAX_MEMORY_PER_POOL_SIZE_SHIFT	(30) // maximum of 1GB per pool
#endif

#define UE_MB3_MAX_MEMORY_PER_POOL_SIZE				(1ull << UE_MB3_MAX_MEMORY_PER_POOL_SIZE_SHIFT) 

// This choice depends on how efficient the OS is with sparse commits in large VM blocks
#if !defined(BINNED3_USE_SEPARATE_VM_PER_POOL)
#	if PLATFORM_WINDOWS
#		define BINNED3_USE_SEPARATE_VM_PER_POOL		(1)
#	else
#		define BINNED3_USE_SEPARATE_VM_PER_POOL		(0)
#	endif
#endif

#define UE_DEFAULT_GBinned3PerThreadCaches			1
#define UE_DEFAULT_GBinned3AllocExtra				32
#define UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle	8

#define UE_MB3_ALLOW_RUNTIME_TWEAKING				UE_MBC_ALLOW_RUNTIME_TWEAKING
#if UE_MB3_ALLOW_RUNTIME_TWEAKING
	extern CORE_API int32 GBinned3PerThreadCaches;
	extern CORE_API int32 GBinned3MaxBundlesBeforeRecycle;
	extern CORE_API int32 GBinned3AllocExtra;
#else
	#define GBinned3PerThreadCaches					UE_DEFAULT_GBinned3PerThreadCaches
	#define GBinned3MaxBundlesBeforeRecycle			UE_DEFAULT_GBinned3MaxBundlesBeforeRecycle
	#define GBinned3AllocExtra						UE_DEFAULT_GBinned3AllocExtra
#endif

#define UE_MB3_ALLOCATOR_STATS						UE_MBC_ALLOCATOR_STATS

#if UE_MB3_ALLOCATOR_STATS
#	define UE_M3_ALLOCATOR_PER_BIN_STATS			!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#else
#	define UE_M3_ALLOCATOR_PER_BIN_STATS			0
#endif


PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS

//
// Optimized virtual memory allocator.
// 
// MallocBinned3 supports two types of allocations - large and small pool allocation:
// 1. For small pool allocation MallocBinned3 reserves contiguous range of virtual memory (a Pool) for each allocation size (a Bin).
//    These Pools can be adjacent to each other in memory if BINNED3_USE_SEPARATE_VM_PER_POOL is set to 0.
//    By default each Pool reserves 1 GB of address space, unless USE_512MB_MAX_MEMORY_PER_BLOCK_SIZE is defined to 1.
//    Each Pool commits and decommits reserved memory in Blocks. Each Block is equal to at least one memory page in it's size.
//    A Block contains N Bins in a way to minimize the tail memory waste. For example 16 bytes bins fit inside 4KB memory page without any waste;
//    i.e. one Block for 16 byte Bins contains 256 bins. However 736 bytes Bin uses two 4KB pages in it's Block to minimize memory waste.
//    Each Pool manages it's Blocks allocation via a Bit Tree (BlocksAllocatedBits\BlocksExhaustedBits members).
//    And every Block manages it's Bins via a number of additional data structures - FPoolInfoSmall and FFreeBlock.
//    FFreeBlock is an in-place header for the Block that's stored at the end of each block and contains info on number of free Bins and index of the next free Block if any
//    Memory is allocated top down in a Block
// 
// 2. For large allocations we go directly to OS, unless UE_MB3_USE_CACHED_PAGE_ALLOCATOR_FOR_LARGE_ALLOCS is defined to 1
//    Each allocation is handled via FPlatformVirtualMemoryBlock and information about it is stored in FPoolInfoLarge
//    FPoolInfoLarge stores the original allocation size and how much memory was committed by the OS in case we need to realloc that allocation and there's enough tail waste to do that in place
//    All FPoolInfoLarge are stored in the PoolHashBucket and HashBucketFreeList
//
class CORE_API FMallocBinned3 : public TMallocBinnedCommon<FMallocBinned3, UE_MB3_MINIMUM_ALIGNMENT, BINNED3_MAX_SMALL_POOL_ALIGNMENT, UE_MB3_MINIMUM_ALIGNMENT_SHIFT, UE_MB3_SMALL_POOL_COUNT, UE_MB3_MAX_SMALL_POOL_SIZE>
{	
	// Forward declarations
	struct FPoolInfoLarge;
	struct FPoolInfoSmall;
	struct FPoolTable;
	using PoolHashBucket = TPoolHashBucket<FPoolInfoLarge>;
	struct Private;


	/** Information about a piece of free memory. */
	struct FFreeBlock
	{
		enum
		{
			CANARY_VALUE = 0xe7
		};

		FORCEINLINE FFreeBlock(uint32 InBlockSize, uint32 InBinSize, uint8 InPoolIndex)
			: BinSizeShifted(InBinSize >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT)
			, PoolIndex(InPoolIndex)
			, Canary(CANARY_VALUE)
			, NextFreeBlockIndex(MAX_uint32)
		{
			check(InPoolIndex < MAX_uint8 && (InBinSize >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT) <= MAX_uint16);
			NumFreeBins = InBlockSize / InBinSize;
		}

		FORCEINLINE uint32 GetNumFreeBins() const
		{
			return NumFreeBins;
		}

		FORCEINLINE bool IsCanaryOk() const
		{
			return Canary == FFreeBlock::CANARY_VALUE;
		}

		FORCEINLINE void CanaryTest() const
		{
			if (!IsCanaryOk())
			{
				CanaryFail();
			}
			//checkSlow(PoolIndex == BoundSizeToPoolIndex(uint32(BinSizeShifted) << UE_MB3_MINIMUM_ALIGNMENT_SHIFT));
		}
		void CanaryFail() const;

		FORCEINLINE void* AllocateBin()
		{
			--NumFreeBins;
			return (uint8*)this + NumFreeBins * (uint32(BinSizeShifted) << UE_MB3_MINIMUM_ALIGNMENT_SHIFT);
		}

		uint16 BinSizeShifted;		// Size of the bins that this list points to >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT
		uint8  PoolIndex;			// Index of this pool
		uint8  Canary;				// Constant value of 0xe3
		uint32 NumFreeBins;			// Number of consecutive free bins here, at least 1.
		uint32 NextFreeBlockIndex;	// Next free block or MAX_uint32
	};

	/** Pool table. */
	struct FPoolTable
	{
		uint32 BinSize;						// Bin size, i.e. 16, 32, 64 etc bytes. Pretty redundant and can be computed
		//uint32 BlockSize;					// Size of a block. A block contains N bins in a way to minimize tail memory waste
		uint32 NumMemoryPagesPerBlock;		// Num memory pages needed to allocate one block. 

		FBitTree BlocksAllocatedBits;		// One bits in here mean the virtual memory is committed
		FBitTree BlocksExhaustedBits;		// One bit in here means the pool is completely full

		uint32 NumEverUsedBlocks;
		FPoolInfoSmall** PoolInfos;			// Book keeping info about every allocated area for this bin's pool

		uint64 UnusedAreaOffsetLow;			// High watermark for allocated vm memory for this pool

#if UE_M3_ALLOCATOR_PER_BIN_STATS
		// these are "head end" stats, above the TLS cache
		TAtomic<int64> TotalRequestedAllocSize;
		TAtomic<int64> TotalAllocCount;
		TAtomic<int64> TotalFreeCount;

		FORCEINLINE void HeadEndAlloc(SIZE_T Size)
		{
			check(Size >= 0 && Size <= BinSize);
			TotalRequestedAllocSize += Size;
			TotalAllocCount++;
		}
		FORCEINLINE void HeadEndFree()
		{
			TotalFreeCount++;
		}
#else
		FORCEINLINE void HeadEndAlloc(SIZE_T Size)
		{
		}
		FORCEINLINE void HeadEndFree()
		{
		}
#endif
	};

	// Pool tables for different pool sizes
	FPoolTable SmallPoolTables[UE_MB3_SMALL_POOL_COUNT];

	uint32 SmallPoolInfosPerPlatformPage;

	PoolHashBucket* HashBuckets;			// Hash buckets for external allocations, reserved in constructor based on the platform constants like page size and virtual address high\low hints
	PoolHashBucket* HashBucketFreeList;		// Hash buckets for allocations that were allocated outside of the platform constants virtual address high\low hints
	uint64 NumLargePoolsPerPage;

	FCriticalSection Mutex;

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	FORCEINLINE uint64 PoolIndexFromPtr(const void* Ptr) const		// returns a uint64 for it can also be used to check if it is an OS allocation
	{
		return (UPTRINT(Ptr) - UPTRINT(Binned3BaseVMPtr)) >> UE_MB3_MAX_MEMORY_PER_POOL_SIZE_SHIFT;
	}
	FORCEINLINE uint8* PoolBasePtr(uint32 InPoolIndex) const
	{
		return Binned3BaseVMPtr + InPoolIndex * UE_MB3_MAX_MEMORY_PER_POOL_SIZE;
	}
#else
#	if UE_MB3_ALLOCATOR_STATS
		void RecordPoolSearch(uint32 Tests) const;
#	else
		FORCEINLINE void RecordPoolSearch(uint32 Tests) const
		{

		}
#	endif

	FORCEINLINE uint64 PoolIndexFromPtr(const void* Ptr) const		// returns a uint64 for it can also be used to check if it is an OS allocation
	{
		if (PoolSearchDiv == 0)
		{
			return (UPTRINT(Ptr) - UPTRINT(PoolBaseVMPtr[0])) >> UE_MB3_MAX_MEMORY_PER_POOL_SIZE_SHIFT;
		}
		uint64 PoolIndex = UE_MB3_SMALL_POOL_COUNT;
		if (((uint8*)Ptr >= PoolBaseVMPtr[0]) & ((uint8*)Ptr < HighestPoolBaseVMPtr + UE_MB3_MAX_MEMORY_PER_POOL_SIZE))
		{
			PoolIndex = uint64((uint8*)Ptr - PoolBaseVMPtr[0]) / PoolSearchDiv;
			if (PoolIndex >= UE_MB3_SMALL_POOL_COUNT)
			{
				PoolIndex = UE_MB3_SMALL_POOL_COUNT - 1;
			}
			uint32 Tests = 1; // we are counting potential cache misses here, not actual comparisons
			if ((uint8*)Ptr < PoolBaseVMPtr[PoolIndex])
			{
				do
				{
					Tests++;
					PoolIndex--;
					check(PoolIndex < UE_MB3_SMALL_POOL_COUNT);
				} while ((uint8*)Ptr < PoolBaseVMPtr[PoolIndex]);
				if ((uint8*)Ptr >= PoolBaseVMPtr[PoolIndex] + UE_MB3_MAX_MEMORY_PER_POOL_SIZE)
				{
					PoolIndex = UE_MB3_SMALL_POOL_COUNT; // was in the gap
				}
			}
			else if ((uint8*)Ptr >= PoolBaseVMPtr[PoolIndex] + UE_MB3_MAX_MEMORY_PER_POOL_SIZE)
			{
				do
				{
					Tests++;
					PoolIndex++;
					check(PoolIndex < UE_MB3_SMALL_POOL_COUNT);
				} while ((uint8*)Ptr >= PoolBaseVMPtr[PoolIndex] + UE_MB3_MAX_MEMORY_PER_POOL_SIZE);
				if ((uint8*)Ptr < PoolBaseVMPtr[PoolIndex])
				{
					PoolIndex = UE_MB3_SMALL_POOL_COUNT; // was in the gap
				}
			}
			RecordPoolSearch(Tests);
		}
		return PoolIndex;
	}

	FORCEINLINE uint8* PoolBasePtr(uint32 InPoolIndex) const
	{
		return PoolBaseVMPtr[InPoolIndex];
	}
#endif //~!BINNED3_USE_SEPARATE_VM_PER_POOL

	FORCEINLINE uint32 PoolIndexFromPtrChecked(const void* Ptr) const
	{
		const uint64 Result = PoolIndexFromPtr(Ptr);
		check(Result < UE_MB3_SMALL_POOL_COUNT);
		return (uint32)Result;
	}

	FORCEINLINE bool IsOSAllocation(const void* Ptr) const
	{
		return PoolIndexFromPtr(Ptr) >= UE_MB3_SMALL_POOL_COUNT;
	}

	FORCEINLINE void* BlockPointerFromContainedPtr(const void* Ptr, uint8 NumMemoryPagesPerBlock, uint32& OutBlockIndex) const
	{
		const uint32 PoolIndex = PoolIndexFromPtrChecked(Ptr);
		uint8* PoolStart = PoolBasePtr(PoolIndex);
		const uint64 BlockIndex = (UPTRINT(Ptr) - UPTRINT(PoolStart)) / (UPTRINT(NumMemoryPagesPerBlock) * UPTRINT(OsAllocationGranularity));
		OutBlockIndex = BlockIndex;

		uint8* Result = PoolStart + BlockIndex * UPTRINT(NumMemoryPagesPerBlock) * UPTRINT(OsAllocationGranularity);

		check(Result < PoolStart + UE_MB3_MAX_MEMORY_PER_POOL_SIZE);
		return Result;
	}

	FORCEINLINE uint8* BlockPointerFromIndecies(uint32 InPoolIndex, uint32 BlockIndex, uint32 BlockSize) const
	{
		uint8* PoolStart = PoolBasePtr(InPoolIndex);
		uint8* Ptr = PoolStart + BlockIndex * uint64(BlockSize);
		check(Ptr + BlockSize <= PoolStart + UE_MB3_MAX_MEMORY_PER_POOL_SIZE);
		return Ptr;
	}

	FPoolInfoSmall* PushNewPoolToFront(FPoolTable& Table, uint32 InBinSize, uint32 InPoolIndex, uint32& OutBlockIndex);
	FPoolInfoSmall* GetFrontPool(FPoolTable& Table, uint32 InPoolIndex, uint32& OutBlockIndex);

public:

	FMallocBinned3();

	virtual ~FMallocBinned3();

	// FMalloc interface.
	virtual bool IsInternallyThreadSafe() const override;
	FORCEINLINE virtual void* Malloc(SIZE_T Size, uint32 Alignment) override
	{
		void* Result = nullptr;
	
		// Only allocate from the small pools if the size is small enough and the alignment isn't crazy large.
		// With large alignments, we'll waste a lot of memory allocating an entire page, but such alignments are highly unlikely in practice.
		if ((Size <= UE_MB3_MAX_SMALL_POOL_SIZE) & (Alignment <= UE_MB3_MINIMUM_ALIGNMENT)) // one branch, not two
		{
			FPerThreadFreeBlockLists* Lists = GBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists)
			{
				const uint32 PoolIndex = BoundSizeToPoolIndex(Size, MemSizeToPoolIndex);
				const uint32 BinSize = PoolIndexToBinSize(PoolIndex);
				Result = Lists->Malloc(PoolIndex);
#if UE_MB3_ALLOCATOR_STATS
				if (Result)
				{
					SmallPoolTables[PoolIndex].HeadEndAlloc(Size);
					Lists->AllocatedMemory += BinSize;
				}
#endif
			}
		}
		if (Result == nullptr)
		{
			Result = MallocExternal(Size, Alignment);
		}

		return Result;
	}

	FORCEINLINE virtual void* Realloc(void* Ptr, SIZE_T NewSize, uint32 Alignment) override
	{
		if (NewSize <= UE_MB3_MAX_SMALL_POOL_SIZE && Alignment <= UE_MB3_MINIMUM_ALIGNMENT) // one branch, not two
		{
			FPerThreadFreeBlockLists* Lists = GBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;

			const uint64 PoolIndex = PoolIndexFromPtr(Ptr);
			if ((!!Lists) & ((!Ptr) | (PoolIndex < UE_MB3_SMALL_POOL_COUNT)))
			{
				uint32 BinSize = 0;

				bool bCanFree = true; // the nullptr is always "freeable"
				if (Ptr)
				{
					// Reallocate to a smaller/bigger pool if necessary
					BinSize = PoolIndexToBinSize(PoolIndex);
					if ((!!NewSize) & (NewSize <= BinSize) & ((!PoolIndex) | (NewSize > PoolIndexToBinSize(static_cast<uint32>(PoolIndex - 1)))))
					{
#if UE_MB3_ALLOCATOR_STATS
						SmallPoolTables[PoolIndex].HeadEndAlloc(NewSize);
						SmallPoolTables[PoolIndex].HeadEndFree();
#endif
						return Ptr;
					}
					bCanFree = Lists->CanFree(PoolIndex, BinSize);
				}
				if (bCanFree)
				{
					const uint32 NewPoolIndex = BoundSizeToPoolIndex(NewSize, MemSizeToPoolIndex);
					const uint32 NewBinSize = PoolIndexToBinSize(NewPoolIndex);
					void* Result = NewSize ? Lists->Malloc(NewPoolIndex) : nullptr;
#if UE_MB3_ALLOCATOR_STATS
					if (Result)
					{
						SmallPoolTables[NewPoolIndex].HeadEndAlloc(NewSize);
						Lists->AllocatedMemory += NewBinSize;
					}
#endif
					if (Result || !NewSize)
					{
						if (Result && Ptr)
						{
							FMemory::Memcpy(Result, Ptr, FPlatformMath::Min<SIZE_T>(NewSize, BinSize));
						}
						if (Ptr)
						{
							const bool bDidPush = Lists->Free(Ptr, PoolIndex, BinSize);
							checkSlow(bDidPush);
#if UE_MB3_ALLOCATOR_STATS
							SmallPoolTables[PoolIndex].HeadEndFree();
							Lists->AllocatedMemory -= BinSize;
#endif
						}

						return Result;
					}
				}
			}
		}
		void* Result = ReallocExternal(Ptr, NewSize, Alignment);
		return Result;
	}

	FORCEINLINE virtual void Free(void* Ptr) override
	{
		const uint64 PoolIndex = PoolIndexFromPtr(Ptr);
		if (PoolIndex < UE_MB3_SMALL_POOL_COUNT)
		{
			FPerThreadFreeBlockLists* Lists = GBinned3PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists)
			{
				const int32 BinSize = PoolIndexToBinSize(PoolIndex);
				if (Lists->Free(Ptr, PoolIndex, BinSize))
				{
#if UE_MB3_ALLOCATOR_STATS
					SmallPoolTables[PoolIndex].HeadEndFree();
					Lists->AllocatedMemory -= BinSize;
#endif
					return;
				}
			}
		}
		FreeExternal(Ptr);
	}

	FORCEINLINE virtual bool GetAllocationSize(void *Ptr, SIZE_T &SizeOut) override
	{
		const uint64 PoolIndex = PoolIndexFromPtr(Ptr);
		if (PoolIndex < UE_MB3_SMALL_POOL_COUNT)
		{
			SizeOut = PoolIndexToBinSize(PoolIndex);
			return true;
		}
		return GetAllocationSizeExternal(Ptr, SizeOut);
	}

	FORCEINLINE virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment) override
	{
		return QuantizeSizeCommon(Count, Alignment, *this);
	}

	virtual bool ValidateHeap() override;
	virtual void Trim(bool bTrimThreadCaches) override;
	virtual void SetupTLSCachesOnCurrentThread() override;
	virtual void MarkTLSCachesAsUsedOnCurrentThread() override;
	virtual void MarkTLSCachesAsUnusedOnCurrentThread() override;
	virtual void ClearAndDisableTLSCachesOnCurrentThread() override;
	virtual const TCHAR* GetDescriptiveName() override;
	// End FMalloc interface.

	void* MallocExternal(SIZE_T Size, uint32 Alignment);
	void* ReallocExternal(void* Ptr, SIZE_T NewSize, uint32 Alignment);
	void FreeExternal(void *Ptr);
	bool GetAllocationSizeExternal(void* Ptr, SIZE_T& SizeOut);

#if UE_MB3_ALLOCATOR_STATS
	int64 GetTotalAllocatedSmallPoolMemory() const;
#endif
	virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats ) override;
	/** Dumps current allocator stats to the log. */
	virtual void DumpAllocatorStats(class FOutputDevice& Ar) override;
	
	// +1 enables PoolIndexToBinSize(~0u / -1) dummy access that helps avoid PoolIndex == 0 branching in Realloc(),
	// see ((!PoolIndex) | (NewSize > PoolIndexToBinSize(static_cast<uint32>(PoolIndex - 1)))
	static uint16 SmallBinSizesReversedShifted[UE_MB3_SMALL_POOL_COUNT + 1]; // this is reversed to get the smallest elements on our main cache line
	static FMallocBinned3* MallocBinned3;
	static uint32 OsAllocationGranularity;

	static void RegisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists);
	static void UnregisterThreadFreeBlockLists(FPerThreadFreeBlockLists* FreeBlockLists);
	static FCriticalSection& GetFreeBlockListsRegistrationMutex();
	static TArray<FPerThreadFreeBlockLists*>& GetRegisteredFreeBlockLists();

#if !BINNED3_USE_SEPARATE_VM_PER_POOL
	static uint8* Binned3BaseVMPtr;
	FPlatformMemory::FPlatformVirtualMemoryBlock Binned3BaseVMBlock;
#else
	static uint64 PoolSearchDiv; // if this is zero, the VM turned out to be contiguous anyway so we use a simple subtract and shift
	static uint8* HighestPoolBaseVMPtr; // this is a duplicate of PoolBaseVMPtr[UE_MB3_SMALL_POOL_COUNT - 1]
	static uint8* PoolBaseVMPtr[UE_MB3_SMALL_POOL_COUNT];
	FPlatformMemory::FPlatformVirtualMemoryBlock PoolBaseVMBlock[UE_MB3_SMALL_POOL_COUNT];
#endif
	// Mapping of sizes to small table indices
	static uint8 MemSizeToPoolIndex[1 + (UE_MB3_MAX_SMALL_POOL_SIZE >> UE_MB3_MINIMUM_ALIGNMENT_SHIFT)];

	FORCEINLINE uint32 PoolIndexToBinSize(uint32 PoolIndex) const
	{
		return uint32(SmallBinSizesReversedShifted[UE_MB3_SMALL_POOL_COUNT - PoolIndex - 1]) << UE_MB3_MINIMUM_ALIGNMENT_SHIFT;
	}

	void FreeBundles(FBundleNode* Bundles, uint32 PoolIndex);
	FCriticalSection& GetMutex() { return Mutex; }

	void Commit(uint32 InPoolIndex, void *Ptr, SIZE_T Size);
	void Decommit(uint32 InPoolIndex, void *Ptr, SIZE_T Size);

	static void* AllocateMetaDataMemory(SIZE_T Size);
	static void FreeMetaDataMemory(void *Ptr, SIZE_T Size);
};

PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS

#define UE_MB3_INLINE (1)
#if UE_MB3_INLINE // during development, it helps with iteration time to not include these here, but rather in the .cpp
	#if PLATFORM_USES_FIXED_GMalloc_CLASS && !FORCE_ANSI_ALLOCATOR && 0/*USE_MALLOC_BINNED3*/
		#define FMEMORY_INLINE_FUNCTION_DECORATOR  FORCEINLINE
		#define FMEMORY_INLINE_GMalloc (FMallocBinned3::MallocBinned3)
		#include "FMemory.inl" // IWYU pragma: export
	#endif
#endif
#endif

