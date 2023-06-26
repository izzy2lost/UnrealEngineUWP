// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ExpandingChunkedList.h: Unreal realtime garbage collection internal helpers
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include <atomic>

template<typename T, int32 NumElementsPerChunk = 64>
class TExpandingChunkedList
{
	struct FChunk
	{
		/** Chunk items */
		T Items[NumElementsPerChunk];
		/** Number of items in this chunk, this can be > NumElementsPerChunk if the chunk is full */
		std::atomic<int32> NumItems = 0;
		/** Pointer to the next chunk */
		FChunk* Next = nullptr;
	};

	/** Head of the chunk list */
	FChunk* Head = nullptr;

public:

	~TExpandingChunkedList()
	{
		Empty();
	}

	/**
	* Thread safe: Pushes a new item into the list
	**/
	FORCEINLINE void Push(T Item)
	{
		FChunk* NewChunk = nullptr;
		FChunk* OldHead = Head;
		FChunk* DestChunk = OldHead;
		int32 Index = DestChunk ? DestChunk->NumItems.fetch_add(1, std::memory_order_acq_rel) : NumElementsPerChunk;
		while (Index >= NumElementsPerChunk)
		{
			// There's no chunks or no free chunks so allocate a new one
			if (!NewChunk)
			{
				// Allocate a new chunk if we haven't already
				NewChunk = new FChunk();
				// reserve index 0 for the new item
				Index = NewChunk->NumItems.fetch_add(1, std::memory_order_acq_rel);
			}
			// Try to swap the head chunk with our new chunk
			FChunk* MaybeDifferentHead = (FChunk*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)&Head, NewChunk, OldHead);
			if (MaybeDifferentHead != OldHead)
			{
				// Someone got here first and added new chunk before we did
				int32 MaybeFreeIndex = MaybeDifferentHead->NumItems.fetch_add(1, std::memory_order_acq_rel);
				if (MaybeFreeIndex < NumElementsPerChunk)
				{
					// And it had a free index so dispose of the chunk we were going to add
					delete NewChunk;
					DestChunk = MaybeDifferentHead;
					Index = MaybeFreeIndex;
				}
				// else keep trying to replace Head with NewChunk in the next iteration
			}
			else
			{
				// We successfully replaced Head with NewChunk
				NewChunk->Next = OldHead;
				DestChunk = NewChunk;
			}
		}

		check(DestChunk != nullptr);
		check(Index >= 0 && Index < NumElementsPerChunk);
		DestChunk->Items[Index] = Item;
	}

	/**
	* Not thread safe: checks if the list is empty
	**/
	FORCEINLINE bool IsEmpty() const
	{
		return Head == nullptr;
	}

	/**
	* Not thread safe: Empties the list and frees its memory
	**/
	void Empty()
	{
		for (FChunk* Chunk = Head; Chunk;)
		{
			FChunk* NextChunk = Chunk->Next;
			delete Chunk;
			Chunk = NextChunk;
		}
		Head = nullptr;
	}

	/**
	* Not thread safe: Moves all items to the provided array and empties the list
	**/
	FORCEINLINE void PopAllAndEmpty(TArray<T>& OutArray)
	{
		for (FChunk* Chunk = Head; Chunk; Chunk = Chunk->Next)
		{
			OutArray.Append(Chunk->Items, FMath::Min(Chunk->NumItems.load(std::memory_order_relaxed), NumElementsPerChunk));
		}
		Empty();
	}
};
