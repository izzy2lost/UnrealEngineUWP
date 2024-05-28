// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/MallocBinnedCommon.h"
#include "Algo/Sort.h"
#include "Containers/ArrayView.h"
#include "Misc/AssertionMacros.h"
#include "Math/NumericLimits.h"
#include "HAL/IConsoleManager.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/UnrealTemplate.h"

PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS

#if PLATFORM_HAS_FPlatformVirtualMemoryBlock

// Block sizes are based around getting the maximum amount of allocations per pool, with as little alignment waste as possible.
// Block sizes should be close to even divisors of the system page size, and well distributed.
// They must be 16-byte aligned as well.
static uint32 BinnedCommonSmallBlockSizes4k[] = 
{
	16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, // +16
	224, 256, 288, 320, // +32
	368,  // /11 ish
	400,  // /10 ish
	448,  // /9 ish
	512,  // /8
	576,  // /7 ish
	672,  // /6 ish
	816,  // /5 ish 
	1024, // /4 
	1360, // /3 ish
	2048, // /2
	4096 // /1
};

static uint32 BinnedCommonSmallBlockSizes8k[] =
{
	736,  // /11 ish
	1168, // /7 ish
	1632, // /5 ish
	2720, // /3 ish
	8192  // /1
};

static uint32 BinnedCommonSmallBlockSizes12k[] =
{
	//1104, // /11 ish
	//1216, // /10 ish
	1536, // /8
	1744, // /7 ish
	2448, // /5 ish
	3072, // /4
	6144, // /2
	12288 // /1
};

static uint32 BinnedCommonSmallBlockSizes16k[] =
{
	//1488, // /11 ish
	//1808, // /9 ish
	//2336, // /7 ish
	3264, // /5 ish
	5456, // /3 ish
	16384 // /1
};

static uint32 BinnedCommonSmallBlockSizes20k[] =
{
	// 2912, // /7 ish
	//3408, // /6 ish
	5120, // /4
	//6186, // /3 ish
	10240, // /2
	20480  // /1
};

static uint32 BinnedCommonSmallBlockSizes24k[] = // 1 total
{
	24576  // /1
};

static uint32 BinnedCommonSmallBlockSizes28k[] = // 6 total
{
	4768,  // /6 ish
	5728,  // /5 ish
	7168,  // /4
	9552,  // /3
	14336, // /2
	28672  // /1
};

FSizeTableEntry::FSizeTableEntry(uint32 InBlockSize, uint64 PlatformPageSize, uint8 Pages4k, uint32 BasePageSize, uint32 MinimumAlignment)
	: BlockSize(InBlockSize)
{
	check((PlatformPageSize & (BasePageSize - 1)) == 0 && PlatformPageSize >= BasePageSize && InBlockSize % MinimumAlignment == 0);

	uint64 Page4kPerPlatformPage = PlatformPageSize / BasePageSize;

	PagesPlatformForBlockOfBlocks = 0;
	while (true)
	{
		check(PagesPlatformForBlockOfBlocks < MAX_uint8);
		PagesPlatformForBlockOfBlocks++;
		if (PagesPlatformForBlockOfBlocks * Page4kPerPlatformPage < Pages4k)
		{
			continue;
		}
		if (PagesPlatformForBlockOfBlocks * Page4kPerPlatformPage % Pages4k != 0)
		{
			continue;
		}
		break;
	}
	check((PlatformPageSize * PagesPlatformForBlockOfBlocks) / BlockSize <= MAX_uint16);
	BlocksPerBlockOfBlocks = (PlatformPageSize * PagesPlatformForBlockOfBlocks) / BlockSize;
}

static inline void FillTable(FSizeTableEntry* SizeTable, int32& Index, const uint32* BlockList, uint32 BlockListSize, uint32 NumPages, uint64 PlatformPageSize, uint32 BasePageSize, uint32 MinimumAlignment)
{
	for (uint32 Sub = 0; Sub < BlockListSize; Sub++)
	{
		// if we override BINNEDCOMMON_MAX_LISTED_SMALL_POOL_SIZE externally, we need to filter out predefined bins of a larger size
		if (BlockList[Sub] <= BINNEDCOMMON_MAX_LISTED_SMALL_POOL_SIZE)
		{
			SizeTable[Index++] = FSizeTableEntry(BlockList[Sub], PlatformPageSize, NumPages, BasePageSize, MinimumAlignment);
		}
	}
}

uint8 FSizeTableEntry::FillSizeTable(uint64 PlatformPageSize, FSizeTableEntry* SizeTable, uint32 BasePageSize, uint32 MinimumAlignment, uint32 MaxSize, uint32 SizeIncrement)
{
	int32 Index = 0;
	FillTable(SizeTable, Index, BinnedCommonSmallBlockSizes4k,  UE_ARRAY_COUNT(BinnedCommonSmallBlockSizes4k),  1, PlatformPageSize, BasePageSize, MinimumAlignment);
	FillTable(SizeTable, Index, BinnedCommonSmallBlockSizes8k,  UE_ARRAY_COUNT(BinnedCommonSmallBlockSizes8k),  2, PlatformPageSize, BasePageSize, MinimumAlignment);
	FillTable(SizeTable, Index, BinnedCommonSmallBlockSizes12k, UE_ARRAY_COUNT(BinnedCommonSmallBlockSizes12k), 3, PlatformPageSize, BasePageSize, MinimumAlignment);
	FillTable(SizeTable, Index, BinnedCommonSmallBlockSizes16k, UE_ARRAY_COUNT(BinnedCommonSmallBlockSizes16k), 4, PlatformPageSize, BasePageSize, MinimumAlignment);
	FillTable(SizeTable, Index, BinnedCommonSmallBlockSizes20k, UE_ARRAY_COUNT(BinnedCommonSmallBlockSizes20k), 5, PlatformPageSize, BasePageSize, MinimumAlignment);
	FillTable(SizeTable, Index, BinnedCommonSmallBlockSizes24k, UE_ARRAY_COUNT(BinnedCommonSmallBlockSizes24k), 6, PlatformPageSize, BasePageSize, MinimumAlignment);
	FillTable(SizeTable, Index, BinnedCommonSmallBlockSizes28k, UE_ARRAY_COUNT(BinnedCommonSmallBlockSizes28k), 7, PlatformPageSize, BasePageSize, MinimumAlignment);

	check(Index == BINNEDCOMMON_NUM_LISTED_SMALL_POOLS);

	Algo::Sort(MakeArrayView(SizeTable, Index));
	check(SizeTable[Index - 1].BlockSize == BINNEDCOMMON_MAX_LISTED_SMALL_POOL_SIZE);
	check(IsAligned(BINNEDCOMMON_MAX_LISTED_SMALL_POOL_SIZE, BasePageSize));
	for (uint32 Size = BINNEDCOMMON_MAX_LISTED_SMALL_POOL_SIZE + BasePageSize; Size <= MaxSize; Size += SizeIncrement)
	{
		SizeTable[Index++] = FSizeTableEntry(Size, PlatformPageSize, Size / BasePageSize, BasePageSize, MinimumAlignment);
	}
	check(Index < 256);
	return (uint8)Index;
}

void FBitTree::FBitTreeInit(uint32 InDesiredCapacity, void * Memory, uint32 MemorySize, bool InitialValue)
{
	Bits = (uint64*)Memory;
	DesiredCapacity = InDesiredCapacity;
	AllocationSize = 8;
	Rows = 1;
	uint32 RowsUint64s = 1;
	Capacity = 64;
	OffsetOfLastRow = 0;

	uint32 RowOffsets[10]; // 10 is way more than enough
	RowOffsets[0] = 0;
	uint32 RowNum[10]; // 10 is way more than enough
	RowNum[0] = 1;

	while (Capacity < DesiredCapacity)
	{
		Capacity *= 64;
		RowsUint64s *= 64;
		OffsetOfLastRow = AllocationSize / 8;
		check(Rows < 10);
		RowOffsets[Rows] = OffsetOfLastRow;
		RowNum[Rows] = RowsUint64s;
		AllocationSize += 8 * RowsUint64s;
		Rows++;
	}

	uint32 LastRowTotal = (AllocationSize - OffsetOfLastRow * 8) * 8;
	uint32 ExtraBits = LastRowTotal - DesiredCapacity;
	AllocationSize -= (ExtraBits / 64) * 8;
	check(AllocationSize <= MemorySize && Bits);

	FMemory::Memset(Bits, InitialValue ? 0xff : 0, AllocationSize);

	if (!InitialValue)
	{
		// we fill everything beyond the desired size with occupied
		uint32 ItemsPerBit = 64;
		for (int32 FillRow = Rows - 2; FillRow >= 0; FillRow--)
		{
			uint32 NeededOneBits = RowNum[FillRow] * 64 - (DesiredCapacity + ItemsPerBit - 1) / ItemsPerBit;
			uint32 NeededOne64s = NeededOneBits / 64;
			NeededOneBits %= 64;
			for (uint32 Fill = RowNum[FillRow] - NeededOne64s; Fill < RowNum[FillRow]; Fill++)
			{
				Bits[RowOffsets[FillRow] + Fill] = MAX_uint64;
			}
			if (NeededOneBits)
			{
				Bits[RowOffsets[FillRow] + RowNum[FillRow] - NeededOne64s - 1] = (MAX_uint64 << (64 - NeededOneBits));
			}
			ItemsPerBit *= 64;
		}

		if (DesiredCapacity % 64)
		{
			Bits[AllocationSize / 8 - 1] = (MAX_uint64 << (DesiredCapacity % 64));
		}
	}
}

uint32 FBitTree::AllocBit()
{
	uint32 Result = MAX_uint32;
	if (*Bits != MAX_uint64) // else we are full
	{
		Result = 0;
		uint32 Offset = 0;
		uint32 Row = 0;
		while (true)
		{
			uint64* At = Bits + Offset;
			check(At >= Bits && At < Bits + AllocationSize / 8);
			uint32 LowestZeroBit = FMath::CountTrailingZeros64(~*At);
			check(LowestZeroBit < 64);
			Result = Result * 64 + LowestZeroBit;
			if (Row == Rows - 1)
			{
				check(!((*At) & (1ull << LowestZeroBit))); // this was already allocated?
				*At |= (1ull << LowestZeroBit);
				if (Row > 0)
				{
					if (*At == MAX_uint64)
					{
						do
						{
							uint32 Rem = (Offset - 1) % 64;
							Offset = (Offset - 1) / 64;
							At = Bits + Offset;
							check(At >= Bits && At < Bits + AllocationSize / 8);
							check(*At != MAX_uint64); // this should not already be marked full
							*At |= (1ull << Rem);
							if (*At != MAX_uint64)
							{
								break;
							}
							Row--;
						} while (Row);
					}
				}
				break;
			}
			Offset = Offset * 64 + 1 + LowestZeroBit;
			Row++;
		}
	}

	return Result;
}

bool FBitTree::IsAllocated(uint32 Index) const
{
	check(Index < DesiredCapacity);
	uint32 Row = Rows - 1;
	uint32 Rem = Index % 64;
	uint32 Offset = OffsetOfLastRow + Index / 64;
	uint64* At = Bits + Offset;
	check(At >= Bits && At < Bits + AllocationSize / 8);
	return !!((*At) & (1ull << Rem));
}

void FBitTree::AllocBit(uint32 Index)
{
	check(Index < DesiredCapacity);
	uint32 Row = Rows - 1;
	uint32 Rem = Index % 64;
	uint32 Offset = OffsetOfLastRow + Index / 64;
	uint64* At = Bits + Offset;
	check(At >= Bits && At < Bits + AllocationSize / 8);
	check(!((*At) & (1ull << Rem))); // this was already allocated?
	*At |= (1ull << Rem);
	if (*At == MAX_uint64 && Row > 0)
	{
		do
		{
			Rem = (Offset - 1) % 64;
			Offset = (Offset - 1) / 64;
			At = Bits + Offset;
			check(At >= Bits && At < Bits + AllocationSize / 8);
			check(!((*At) & (1ull << Rem))); // this was already allocated?
			*At |= (1ull << Rem);
			if (*At != MAX_uint64)
			{
				break;
			}
			Row--;
		} while (Row);
	}

}
uint32 FBitTree::NextAllocBit() const
{
	uint32 Result = MAX_uint32;
	if (*Bits != MAX_uint64) // else we are full
	{
		Result = 0;
		uint32 Offset = 0;
		uint32 Row = 0;
		while (true)
		{
			uint64* At = Bits + Offset;
			check(At >= Bits && At < Bits + AllocationSize / 8);
			uint32 LowestZeroBit = FMath::CountTrailingZeros64(~*At);
			check(LowestZeroBit < 64);
			Result = Result * 64 + LowestZeroBit;
			if (Row == Rows - 1)
			{
				check(!((*At) & (1ull << LowestZeroBit))); // this was already allocated?
				break;
			}
			Offset = Offset * 64 + 1 + LowestZeroBit;
			Row++;
		}
	}

	return Result;
}

uint32 FBitTree::NextAllocBit(uint32 StartIndex) const
{
	if (*Bits != MAX_uint64) // else we are full
	{
		uint32 Index = StartIndex;
		check(Index < DesiredCapacity);
		uint32 Row = Rows - 1;
		uint32 Rem = Index % 64;
		uint32 Offset = OffsetOfLastRow + Index / 64;
		uint64* At = Bits + Offset;
		check(At >= Bits && At < Bits + AllocationSize / 8);
		uint64 LocalAt = *At;
		if (!(LocalAt & (1ull << Rem)))
		{
			return Index; // lucked out, start was unallocated
		}
		// start was allocated, search for an unallocated one
		LocalAt |= MAX_uint64 >> (63 - Rem); // set and ignore the bits representing items before (and including) the start
		if (LocalAt != MAX_uint64)
		{
			// this qword has an item we can use
			uint32 LowestZeroBit = FMath::CountTrailingZeros64(~LocalAt);
			check(LowestZeroBit < 64);
			return Index - Rem + LowestZeroBit;

		}
		// rest of qword was also allocated, search up the tree for the next free item
		if (Row > 0)
		{
			do
			{
				Row--;
				Rem = (Offset - 1) % 64;
				Offset = (Offset - 1) / 64;
				At = Bits + Offset;
				check(At >= Bits && At < Bits + AllocationSize / 8);
				LocalAt = *At;
				LocalAt |= MAX_uint64 >> (63 - Rem); // set and ignore the bits representing items before (and including) the start
				if (LocalAt != MAX_uint64)
				{
					// this qword has an item we can use
					// now search down the tree
					while (true)
					{
						uint32 LowestZeroBit = FMath::CountTrailingZeros64(~LocalAt);
						check(LowestZeroBit < 64);
						if (Row == Rows - 1)
						{
							check(!(LocalAt & (1ull << LowestZeroBit))); // this was already allocated?
							uint32 Result = (Offset - OffsetOfLastRow) * 64 + LowestZeroBit;
							check(Result < DesiredCapacity);
							return Result;

						}
						Offset = Offset * 64 + 1 + LowestZeroBit;
						At = Bits + Offset;
						check(At >= Bits && At < Bits + AllocationSize / 8);
						LocalAt = *At;
						Row++;
					}
				}
			} while (Row);
		}
	}

	return MAX_uint32;
}


void FBitTree::FreeBit(uint32 Index)
{
	check(Index < DesiredCapacity);
	uint32 Row = Rows - 1;
	uint32 Rem = Index % 64;
	uint32 Offset = OffsetOfLastRow + Index / 64;
	uint64* At = Bits + Offset;
	check(At >= Bits && At < Bits + AllocationSize / 8);
	bool bWasFull = *At == MAX_uint64;
	check((*At) & (1ull << Rem)); // this was not already allocated?
	*At &= ~(1ull << Rem);
	if (bWasFull && Row > 0)
	{
		do
		{
			Rem = (Offset - 1) % 64;
			Offset = (Offset - 1) / 64;
			At = Bits + Offset;
			check(At >= Bits && At < Bits + AllocationSize / 8);
			bWasFull = *At == MAX_uint64;
			*At &= ~(1ull << Rem);
			if (!bWasFull)
			{
				break;
			}
			Row--;
		} while (Row);
	}
}

uint32 FBitTree::CountOnes(uint32 UpTo) const
{
	uint32 Result = 0;
	uint64* At = Bits + OffsetOfLastRow;
	while (UpTo >= 64)
	{
		Result += FMath::CountBits(*At);
		At++;
		UpTo -= 64;
	}
	if (UpTo)
	{
		Result += FMath::CountBits((*At) << (64 - UpTo));
	}
	return Result;
}

/**
 * Finds a contiguous span of unallocated bits.
 * NumBits must be a power of two or a multiple of 64.
 * Only checks regions aligned to min(NumBits, 64).
 * 
 * Warning, slow!
 * Requires a linear search along the bottom row! O(Capacity / min(NumBits,64)) iterations.
 * 
 * Returns the index of the first unallocated bit in the span.
 */
uint32 FBitTree::Slow_NextAllocBits(uint32 NumBits, uint64 StartIndex)
{
	check(FMath::IsPowerOfTwo(NumBits) || NumBits % 64 == 0);

	uint32 Offset = OffsetOfLastRow + StartIndex / 64;
	uint32 MaxOffset = OffsetOfLastRow + DesiredCapacity / 64;

	// If the number of bits is >= 64, we can search int-by-int instead of bit-by-bit on the lowest row
	if (NumBits >= 64)
	{
		uint32 NumInts = NumBits / 64; // Number of uint64s required to hold our allocation
		uint32 FreeInts = 0; // Number of free uint64s we've encountered in a row
		
		while (Offset < MaxOffset)
		{
			// Increment FreeInts if the uint64 is empty, otherwise reset to 0
			FreeInts = Bits[Offset] ? 0 : (FreeInts + 1);

			if (FreeInts == NumInts)
			{
				// Offset will be pointing at the last uint64 in the free span - correct to point at the first
				return ((Offset - OffsetOfLastRow) - (NumInts - 1)) * 64;
			}
			
			Offset++;
		}
	}

	// If the number of bits is < 64, we need to loop over ints and then power of two bits within those ints
	else
	{
		uint32 SlotsPerInt = 64 / NumBits; // How many possible positions for our allocation there are per uint64

		while (Offset < MaxOffset)
		{
			// Allocation of bits within a given uint64 goes RIGHT-TO-LEFT
			// Create right-aligned mask based on number of bits
			uint64 Mask = (1ull << NumBits) - 1;

			// Shift that mask left to check each aligned section of the int
			// Return offset + bit position if we find a free section
			for (uint32 i = 0; i < SlotsPerInt; i++)
			{
				if (!(Mask & Bits[Offset]))
				{
					uint64 Result = (Offset - OffsetOfLastRow) * 64 + (i * NumBits);

					// Ensure we don't return results before our requested start index
					if (Result >= StartIndex) {
						return Result;
					}
				}

				// Shift mask left
				Mask <<= NumBits;
			}

			Offset++;
		}
	}

	check(false); // If we get here, no aligned span of this size is available
	return MAX_uint32;
}

#endif

float GMallocBinnedFlushThreadCacheMaxWaitTime = 0.2f;
static FAutoConsoleVariableRef GMallocBinnedFlushThreadCacheMaxWaitTimeCVar(
	TEXT("MallocBinned.FlushThreadCacheMaxWaitTime"),
	GMallocBinnedFlushThreadCacheMaxWaitTime,
	TEXT("The threshold of time before warning about FlushCurrentThreadCache taking too long (seconds)."),
	ECVF_ReadOnly
);

int32 GMallocBinnedFlushRegisteredThreadCachesOnOneThread = 1;
static FAutoConsoleVariableRef GMallocBinnedFlushRegisteredThreadCachesOnOneThreadCVar(
	TEXT("MallocBinned.FlushRegisteredThreadCachesOnOneThread"),
	GMallocBinnedFlushRegisteredThreadCachesOnOneThread,
	TEXT("Wether or not to attempt to flush registered thread caches on one thread (enabled by default)."));

#if UE_BINNEDCOMMON_ALLOW_RUNTIME_TWEAKING

int32 GMallocBinnedBundleSize = DEFAULT_GMallocBinnedBundleSize;
static FAutoConsoleVariableRef GMallocBinned3BundleSizeCVar(
	TEXT("MallocBinned.BundleSize"),
	GMallocBinnedBundleSize,
	TEXT("Max size in bytes of per-block bundles used in the recycling process")
);

int32 GMallocBinnedBundleCount = DEFAULT_GMallocBinnedBundleCount;
static FAutoConsoleVariableRef GMallocBinned3BundleCountCVar(
	TEXT("MallocBinned.BundleCount"),
	GMallocBinnedBundleCount,
	TEXT("Max count in blocks per-block bundles used in the recycling process")
);

#endif

uint32 FMallocBinnedCommonBase::BinnedTlsSlot = FPlatformTLS::InvalidTlsSlot;
#if UE_BINNEDCOMMON_ALLOCATOR_STATS
std::atomic<int64> FMallocBinnedCommonBase::TLSMemory(0);
std::atomic<int64> FMallocBinnedCommonBase::ConsolidatedMemory(0);
#endif

PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS
