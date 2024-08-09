// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/MemoryBase.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/PlatformTLS.h"
#include "Async/Mutex.h"
#include "Templates/AlignmentTemplates.h"

#include <atomic>

#if PLATFORM_HAS_FPlatformVirtualMemoryBlock

#include "HAL/PlatformMemory.h"

// A project can define it's own UE_MBC_MAX_LISTED_SMALL_POOL_SIZE and UE_MBC_NUM_LISTED_SMALL_POOLS to reduce runtime memory usage
// MallocBinnedCommon.cpp has a list of predefined bins that go up to 28672
// By default allocators (i.e. MB3) that use these bins will rely on this number as a baseline for a small bins count
// These allocators can increase the amount of small bins they want to manage by going over the default MBC bins list
// In MB3 case that means defining BINNED3_MAX_SMALL_POOL_SIZE to something like 65536
// Every bin over the UE_MBC_MAX_LISTED_SMALL_POOL_SIZE would come with a 4kb increment
// These small bins would be kept in user mode, increasing application's memory footprint and reducing the time it takes to allocate memory from the said bins
// If application needs to aggressively reduce it's memory footprint, potentially trading some perf due to an increased amount of kernel calls to allocate memory
// it can redefine UE_MBC_MAX_LISTED_SMALL_POOL_SIZE and BINNED3_MAX_SMALL_POOL_SIZE to smaller numbers, the good value is 16384 for both
// This, however, would require the app to redefine UE_MBC_NUM_LISTED_SMALL_POOLS too to match the number of bins that fall under the new define's threshold
// In case of 16384, we'll skip 3 larger bins and so UE_MBC_NUM_LISTED_SMALL_POOLS should be set to 48 at the time of writing
#if !defined(UE_MBC_MAX_LISTED_SMALL_POOL_SIZE)
#	define UE_MBC_MAX_LISTED_SMALL_POOL_SIZE	28672
#endif

#if !defined(UE_MBC_NUM_LISTED_SMALL_POOLS)
#	define UE_MBC_NUM_LISTED_SMALL_POOLS	51
#endif

#if !defined(BINNEDCOMMON_USE_SEPARATE_VM_PER_POOL)
#	if PLATFORM_WINDOWS
#		define BINNEDCOMMON_USE_SEPARATE_VM_PER_POOL (1)
#	else
#		define BINNEDCOMMON_USE_SEPARATE_VM_PER_POOL (0)
#	endif
#endif


class FBitTree
{
	uint64* Bits; // one bits in middle layers mean "all allocated"
	uint32 Capacity; // rounded up to a power of two
	uint32 DesiredCapacity;
	uint32 Rows;
	uint32 OffsetOfLastRow;
	uint32 AllocationSize;

public:
	FBitTree()
		: Bits(nullptr)
	{
	}

	static constexpr uint32 GetMemoryRequirements(uint32 NumPages)
	{
		uint32 AllocationSize = 8;
		uint32 RowsUint64s = 1;
		uint32 Capacity = 64;
		uint32 OffsetOfLastRow = 0;

		while (Capacity < NumPages)
		{
			Capacity *= 64;
			RowsUint64s *= 64;
			OffsetOfLastRow = AllocationSize / 8;
			AllocationSize += 8 * RowsUint64s;
		}

		uint32 LastRowTotal = (AllocationSize - OffsetOfLastRow * 8) * 8;
		uint32 ExtraBits = LastRowTotal - NumPages;
		AllocationSize -= (ExtraBits / 64) * 8;
		return AllocationSize;
	}

	void FBitTreeInit(uint32 InDesiredCapacity, void * Memory, uint32 MemorySize, bool InitialValue);
	uint32 AllocBit();
	bool IsAllocated(uint32 Index) const;
	void AllocBit(uint32 Index);
	uint32 NextAllocBit() const;
	uint32 NextAllocBit(uint32 StartIndex) const;
	void FreeBit(uint32 Index);
	uint32 CountOnes(uint32 UpTo) const;

	uint32 Slow_NextAllocBits(uint32 NumBits, uint64 StartIndex); // Warning, slow! NumBits must be a power of two or a multiple of 64.
};

struct FSizeTableEntry
{
	uint32 BinSize;
	uint32 NumMemoryPagesPerBlock;

	FSizeTableEntry() = default;
	FSizeTableEntry(uint32 InBinSize, uint64 PlatformPageSize, uint8 Num4kbPages, uint32 BasePageSize, uint32 MinimumAlignment);

	bool operator<(const FSizeTableEntry& Other) const
	{
		return BinSize < Other.BinSize;
	}
	static uint8 FillSizeTable(uint64 PlatformPageSize, FSizeTableEntry* SizeTable, uint32 BasePageSize, uint32 MinimumAlignment, uint32 MaxSize, uint32 SizeIncrement);
};

#endif	//~PLATFORM_HAS_FPlatformVirtualMemoryBlock


#if !defined(AGGRESSIVE_MEMORY_SAVING)
#	error "AGGRESSIVE_MEMORY_SAVING must be defined"
#endif

#if AGGRESSIVE_MEMORY_SAVING
#	define UE_DEFAULT_GMallocBinnedBundleSize 8192
#else
#	define UE_DEFAULT_GMallocBinnedBundleSize 65536
#endif

#define UE_DEFAULT_GMallocBinnedBundleCount 64

#ifndef UE_MBC_ALLOW_RUNTIME_TWEAKING
#	define UE_MBC_ALLOW_RUNTIME_TWEAKING 0
#endif

#if UE_MBC_ALLOW_RUNTIME_TWEAKING
	extern CORE_API int32 GMallocBinnedBundleSize;
	extern CORE_API int32 GMallocBinnedBundleCount;
#else
#	define GMallocBinnedBundleSize	UE_DEFAULT_GMallocBinnedBundleSize
#	define GMallocBinnedBundleCount	UE_DEFAULT_GMallocBinnedBundleSize
#endif

#ifndef UE_MBC_ALLOCATOR_STATS
#	define UE_MBC_ALLOCATOR_STATS (!UE_BUILD_SHIPPING || WITH_EDITOR)
#endif

extern CORE_API float GMallocBinnedFlushThreadCacheMaxWaitTime;
extern CORE_API int32 GMallocBinnedFlushRegisteredThreadCachesOnOneThread;

class FMallocBinnedCommonBase : public FMalloc
{
protected:
	struct FPtrToPoolMapping
	{
		FPtrToPoolMapping()
			: PtrToPoolPageBitShift(0)
			, HashKeyShift(0)
			, PoolMask(0)
			, MaxHashBuckets(0)
			, AddressSpaceBase(0)
		{
		}

		explicit FPtrToPoolMapping(uint32 InPageSize, uint64 InNumPoolsPerPage, uint64 AddressBase, uint64 AddressLimit)
		{
			Init(InPageSize, InNumPoolsPerPage, AddressBase, AddressLimit);
		}

		void Init(uint32 InPageSize, uint64 InNumPoolsPerPage, uint64 AddressBase, uint64 AddressLimit)
		{
			const uint64 PoolPageToPoolBitShift = FPlatformMath::CeilLogTwo64(InNumPoolsPerPage);

			PtrToPoolPageBitShift = FPlatformMath::CeilLogTwo(InPageSize);
			HashKeyShift = PtrToPoolPageBitShift + PoolPageToPoolBitShift;
			PoolMask = (1ull << PoolPageToPoolBitShift) - 1;
			MaxHashBuckets = FMath::RoundUpToPowerOfTwo64(AddressLimit - AddressBase) >> HashKeyShift;
			AddressSpaceBase = AddressBase;
		}

		FORCEINLINE void GetHashBucketAndPoolIndices(const void* InPtr, uint32& OutBucketIndex, UPTRINT& OutBucketCollision, uint32& OutPoolIndex) const
		{
			check((UPTRINT)InPtr >= AddressSpaceBase);
			const UPTRINT Ptr = (UPTRINT)InPtr - AddressSpaceBase;
			OutBucketCollision = Ptr >> HashKeyShift;
			OutBucketIndex = uint32(OutBucketCollision & (MaxHashBuckets - 1));
			OutPoolIndex = uint32((Ptr >> PtrToPoolPageBitShift) & PoolMask);
		}

		FORCEINLINE uint64 GetMaxHashBuckets() const
		{
			return MaxHashBuckets;
		}

	private:
		/** Shift to apply to a pointer to get the reference from the indirect tables */
		uint64 PtrToPoolPageBitShift;

		/** Shift required to get required hash table key. */
		uint64 HashKeyShift;

		/** Used to mask off the bits that have been used to lookup the indirect table */
		uint64 PoolMask;

		// PageSize dependent constants
		uint64 MaxHashBuckets;

		// Base address for any virtual allocations. Can be non 0 on some platforms
		uint64 AddressSpaceBase;
	};

	// This needs to be small enough to fit inside the smallest allocation handled by MallocBinned2\3, hence the union.
	struct FBundleNode
	{
		FBundleNode* NextNodeInCurrentBundle;

		// NextBundle ptr is valid when node is stored in FFreeBlockList in a thread-local list of reusable allocations.
		// Count is valid when node is stored in global recycler and caches the number of nodes in the list formed by NextNodeInCurrentBundle.
		union
		{
			FBundleNode* NextBundle;
			int32 Count;
		};
	};

	struct FBundle
	{
		FORCEINLINE FBundle()
		{
			Reset();
		}

		FORCEINLINE void Reset()
		{
			Head = nullptr;
			Count = 0;
		}

		FORCEINLINE void PushHead(FBundleNode* Node)
		{
			Node->NextNodeInCurrentBundle = Head;
			Node->NextBundle = nullptr;
			Head = Node;
			Count++;
		}

		FORCEINLINE FBundleNode* PopHead()
		{
			FBundleNode* Result = Head;

			Count--;
			Head = Head->NextNodeInCurrentBundle;
			return Result;
		}

		FBundleNode* Head;
		uint32       Count;
	};

	/** Hash table struct for retrieving allocation book keeping information */
	template <class T>
	struct TPoolHashBucket
	{
		UPTRINT			 BucketIndex;
		T*				 FirstPool;
		TPoolHashBucket* Prev;
		TPoolHashBucket* Next;

		TPoolHashBucket()
		{
			BucketIndex = 0;
			FirstPool = nullptr;
			Prev = this;
			Next = this;
		}

		void Link(TPoolHashBucket* After)
		{
			After->Prev = Prev;
			After->Next = this;
			Prev->Next = After;
			this->Prev = After;
		}

		void Unlink()
		{
			Next->Prev = Prev;
			Prev->Next = Next;
			Prev = this;
			Next = this;
		}
	};

	FPtrToPoolMapping PtrToPoolMapping;
	static CORE_API uint32 BinnedTlsSlot;

#if UE_MBC_ALLOCATOR_STATS
	static std::atomic<int64> TLSMemory;
	static std::atomic<int64> ConsolidatedMemory;
#endif
	std::atomic<uint64> MemoryTrimEpoch{ 0 };
};

template <class AllocType, int MinAlign, int MaxAlign, int MinAlignShift, int NumSmallPools, int MaxSmallPoolSize>
class TMallocBinnedCommon : public FMallocBinnedCommonBase
{
	static_assert(sizeof(FBundleNode) <= MinAlign, "Bundle nodes must fit into the smallest block size");
	friend class FMallocBinnedCommonUtils;

	static constexpr int MIN_ALIGN           = MinAlign;
	static constexpr int MAX_ALIGN           = MaxAlign;
	static constexpr int MIN_ALIGN_SHIFT     = MinAlignShift;
	static constexpr int NUM_SMALL_POOLS     = NumSmallPools;
	static constexpr int MAX_SMALL_POOL_SIZE = MaxSmallPoolSize;
	
protected:
	struct FFreeBlockList
	{
		// return true if we actually pushed it
		FORCEINLINE bool PushToFront(void* InPtr, uint32 InPoolIndex, uint32 InBinSize)
		{
			checkSlow(InPtr);

			if ((PartialBundle.Count >= (uint32)GMallocBinnedBundleCount) | (PartialBundle.Count * InBinSize >= (uint32)GMallocBinnedBundleSize))
			{
				if (FullBundle.Head)
				{
					return false;
				}
				FullBundle = PartialBundle;
				PartialBundle.Reset();
			}
			PartialBundle.PushHead((FBundleNode*)InPtr);
			return true;
		}

		FORCEINLINE bool CanPushToFront(uint32 InPoolIndex, uint32 InBinSize) const
		{
			return !((!!FullBundle.Head) & ((PartialBundle.Count >= (uint32)GMallocBinnedBundleCount) | (PartialBundle.Count * InBinSize >= (uint32)GMallocBinnedBundleSize)));
		}

		FORCEINLINE void* PopFromFront(uint32 InPoolIndex)
		{
			if ((!PartialBundle.Head) & (!!FullBundle.Head))
			{
				PartialBundle = FullBundle;
				FullBundle.Reset();
			}
			return PartialBundle.Head ? PartialBundle.PopHead() : nullptr;
		}

		// tries to recycle the full bundle, if that fails, it is returned for freeing
		template <class T>
		FBundleNode* RecyleFull(uint32 InPoolIndex, T& InGlobalRecycler)
		{
			FBundleNode* Result = nullptr;
			if (FullBundle.Head)
			{
				FullBundle.Head->Count = FullBundle.Count;
				if (!InGlobalRecycler.PushBundle(InPoolIndex, FullBundle.Head))
				{
					Result = FullBundle.Head;
					Result->NextBundle = nullptr;
				}
				FullBundle.Reset();
			}
			return Result;
		}

		template <class T>
		bool ObtainPartial(uint32 InPoolIndex, T& InGlobalRecycler)
		{
			if (!PartialBundle.Head)
			{
				PartialBundle.Count = 0;
				PartialBundle.Head = InGlobalRecycler.PopBundle(InPoolIndex);
				if (PartialBundle.Head)
				{
					PartialBundle.Count = PartialBundle.Head->Count;
					PartialBundle.Head->NextBundle = nullptr;
					return true;
				}
				return false;
			}
			return true;
		}

		FBundleNode* PopBundles(uint32 InPoolIndex)
		{
			FBundleNode* Partial = PartialBundle.Head;
			if (Partial)
			{
				PartialBundle.Reset();
				Partial->NextBundle = nullptr;
			}

			FBundleNode* Full = FullBundle.Head;
			if (Full)
			{
				FullBundle.Reset();
				Full->NextBundle = nullptr;
			}

			FBundleNode* Result = Partial;
			if (Result)
			{
				Result->NextBundle = Full;
			}
			else
			{
				Result = Full;
			}

			return Result;
		}

	private:
		FBundle PartialBundle;
		FBundle FullBundle;
	};

	struct FPerThreadFreeBlockLists
	{
		FORCEINLINE static FPerThreadFreeBlockLists* Get() TSAN_SAFE
		{
			FPerThreadFreeBlockLists* ThreadSingleton = FPlatformTLS::IsValidTlsSlot(BinnedTlsSlot) ? (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(BinnedTlsSlot) : nullptr;
			// If the current thread doesn't have the Lock, we can't return the TLS cache for being used on the current thread as we risk racing with another thread doing trimming.
			// This can only happen in such a scenario.
			//
			//  FMemory::MarkTLSCachesAsUnusedOnCurrentThread();
			//  Node->Event->Wait(); <----- UNSAFE to use the TLS cache by its owner thread but can happen when the wait implementation allocates or frees something.
			//  FMemory::MarkTLSCachesAsUsedOnCurrentThread();
			if (ThreadSingleton && ThreadSingleton->bLockedByOwnerThread)
			{
				return ThreadSingleton;
			}
			return nullptr;
		}

		static void SetTLS()
		{
			check(FPlatformTLS::IsValidTlsSlot(BinnedTlsSlot));
			FPerThreadFreeBlockLists* ThreadSingleton = (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(BinnedTlsSlot);
			if (!ThreadSingleton)
			{
				const int64 TLSSize = Align(sizeof(FPerThreadFreeBlockLists), AllocType::OsAllocationGranularity);
				ThreadSingleton = new (AllocType::AllocateMetaDataMemory(TLSSize)) FPerThreadFreeBlockLists();
#if UE_MBC_ALLOCATOR_STATS
				TLSMemory.fetch_add(TLSSize, std::memory_order_relaxed);
#endif
				verify(ThreadSingleton);
				ThreadSingleton->bLockedByOwnerThread = true;
				ThreadSingleton->Lock();
				FPlatformTLS::SetTlsValue(BinnedTlsSlot, ThreadSingleton);
				AllocType::RegisterThreadFreeBlockLists(ThreadSingleton);
			}
		}

		static void UnlockTLS()
		{
			FPerThreadFreeBlockLists* ThreadSingleton = (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(BinnedTlsSlot);
			if (ThreadSingleton)
			{
				ThreadSingleton->bLockedByOwnerThread = false;
				ThreadSingleton->Unlock();
			}
		}

		static void LockTLS()
		{
			FPerThreadFreeBlockLists* ThreadSingleton = (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(BinnedTlsSlot);
			if (ThreadSingleton)
			{
				ThreadSingleton->Lock();
				ThreadSingleton->bLockedByOwnerThread = true;
			}
		}

		static void ClearTLS()
		{
			check(FPlatformTLS::IsValidTlsSlot(BinnedTlsSlot));
			FPerThreadFreeBlockLists* ThreadSingleton = (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(BinnedTlsSlot);
			if (ThreadSingleton)
			{
				const int64 TLSSize = Align(sizeof(FPerThreadFreeBlockLists), AllocType::OsAllocationGranularity);
#if UE_MBC_ALLOCATOR_STATS
				TLSMemory.fetch_sub(TLSSize, std::memory_order_relaxed);
#endif
				AllocType::UnregisterThreadFreeBlockLists(ThreadSingleton);
				ThreadSingleton->bLockedByOwnerThread = false;
				ThreadSingleton->Unlock();
				ThreadSingleton->~FPerThreadFreeBlockLists();

				AllocType::FreeMetaDataMemory(ThreadSingleton, TLSSize);
			}
			FPlatformTLS::SetTlsValue(BinnedTlsSlot, nullptr);
		}

		FORCEINLINE void* Malloc(uint32 InPoolIndex)
		{
			return FreeLists[InPoolIndex].PopFromFront(InPoolIndex);
		}

		// return true if the pointer was pushed
		FORCEINLINE bool Free(void* InPtr, uint32 InPoolIndex, uint32 InBinSize)
		{
			return FreeLists[InPoolIndex].PushToFront(InPtr, InPoolIndex, InBinSize);
		}

		// return true if a pointer can be pushed
		FORCEINLINE bool CanFree(uint32 InPoolIndex, uint32 InBinSize) const
		{
			return FreeLists[InPoolIndex].CanPushToFront(InPoolIndex, InBinSize);
		}

		// returns a bundle that needs to be freed if it can't be recycled
		template <class T>
		FBundleNode* RecycleFullBundle(uint32 InPoolIndex, T& InGlobalRecycler)
		{
			return FreeLists[InPoolIndex].RecyleFull(InPoolIndex, InGlobalRecycler);
		}

		// returns true if we have anything to pop
		template <class T>
		bool ObtainRecycledPartial(uint32 InPoolIndex, T& InGlobalRecycler)
		{
			return FreeLists[InPoolIndex].ObtainPartial(InPoolIndex, InGlobalRecycler);
		}

		FBundleNode* PopBundles(uint32 InPoolIndex)
		{
			return FreeLists[InPoolIndex].PopBundles(InPoolIndex);
		}

		void Lock()
		{
			Mutex.Lock();
		}

		bool TryLock()
		{
			return Mutex.TryLock();
		}

		void Unlock()
		{
			Mutex.Unlock();
		}

		// should only be called from inside the Lock.
		bool UpdateEpoch(uint64 NewEpoch)
		{
			if (MemoryTrimEpoch >= NewEpoch)
			{
				return false;
			}

			MemoryTrimEpoch = NewEpoch;
			return true;
		}

#if UE_MBC_ALLOCATOR_STATS
	public:
		int64 AllocatedMemory = 0;
#endif
	private:
		UE::FMutex Mutex;
		uint64 MemoryTrimEpoch = 0;
		FFreeBlockList FreeLists[NumSmallPools];
		bool bLockedByOwnerThread = false;
	};

	FORCEINLINE SIZE_T QuantizeSizeCommon(SIZE_T Count, uint32 Alignment, const AllocType& Alloc) const
	{
		static_assert(DEFAULT_ALIGNMENT <= MinAlign, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below
		checkSlow(FMath::IsPowerOfTwo(Alignment));
		SIZE_T SizeOut;
		if ((Count <= MaxSmallPoolSize) & (Alignment <= MinAlign)) // one branch, not two
		{
			SizeOut = Alloc.PoolIndexToBinSize(BoundSizeToPoolIndex(Count, Alloc.MemSizeToPoolIndex));
			check(SizeOut >= Count);
			return SizeOut;
		}
		Alignment = FMath::Max<uint32>(Alignment, MinAlign);
		Count = Align(Count, Alignment);
		if ((Count <= MaxSmallPoolSize) & (Alignment <= MaxAlign))
		{
			uint32 PoolIndex = BoundSizeToPoolIndex(Count, Alloc.MemSizeToPoolIndex);
			do
			{
				const uint32 BinSize = Alloc.PoolIndexToBinSize(PoolIndex);
				if (IsAligned(BinSize, Alignment))
				{
					SizeOut = SIZE_T(BinSize);
					check(SizeOut >= Count);
					return SizeOut;
				}

				PoolIndex++;
			} while (PoolIndex < NumSmallPools);
		}

		Alignment = FPlatformMath::Max<uint32>(Alignment, Alloc.OsAllocationGranularity);
		SizeOut = Align(Count, Alignment);
		check(SizeOut >= Count);
		return SizeOut;
	}

	FORCEINLINE uint32 BoundSizeToPoolIndex(SIZE_T Size, const uint8(&MemSizeToPoolIndex)[1 + (MaxSmallPoolSize >> MinAlignShift)]) const
	{
		const auto Index = ((Size + MinAlign - 1) >> MinAlignShift);
		checkSlow(Index >= 0 && Index <= (MaxSmallPoolSize >> MinAlignShift)); // and it should be in the table
		const uint32 PoolIndex = uint32(MemSizeToPoolIndex[Index]);
		checkSlow(PoolIndex >= 0 && PoolIndex < NumSmallPools);
		return PoolIndex;
	}

	bool PromoteToLargerBin(SIZE_T& Size, uint32& Alignment, const AllocType& Alloc) const
	{
		// try to promote our allocation request to a larger bin with a matching natural alignment
		// if requested alignment is larger than MinAlign but smaller than MaxAlign
		// so we don't do a page allocation with a lot of memory waste
		Alignment = FMath::Max<uint32>(Alignment, MinAlign);
		const SIZE_T AlignedSize = Align(Size, Alignment);
		if (UNLIKELY((AlignedSize <= MaxSmallPoolSize) && (Alignment <= MaxAlign)))
		{
			uint32 PoolIndex = BoundSizeToPoolIndex(AlignedSize, Alloc.MemSizeToPoolIndex);
			do
			{
				const uint32 BlockSize = Alloc.PoolIndexToBinSize(PoolIndex);
				if (IsAligned(BlockSize, Alignment))
				{
					// we found a matching pool for our alignment and size requirements, so modify the size request to match
					Size = SIZE_T(BlockSize);
					Alignment = MinAlign;
					return true;
				}

				PoolIndex++;
			} while (PoolIndex < NumSmallPools);
		}

		return false;
	}
};