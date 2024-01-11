// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseScratchBuffer.h"

// Set to 1 to run additional tests on the scratch buffer. These are normally to expensive to run, but can help find deeper issues with the
// scratch buffer.
#define TEDS_SCRATCHBUFFER_ENABLE_ADDITIONAL_TESTS 0

FTypedElementDatabaseScratchBuffer::~FTypedElementDatabaseScratchBuffer()
{
	// This assumes it's being called after all threads using the scratch buffer have been shutdown already so the
	// scratch buffers they hold have been released back into the pool.

	NextFrame(); // Force to next frame so any lingering blocks are freed correctly.
	RecycleBlocks();
	checkf(FullBlocks == nullptr, TEXT("Not all full blocks were recycled. This can lead to memory leaks."));
	
	FBlock* FrontAvailable = AvailableBlocks;
	while (FrontAvailable)
	{
		FBlock* Next = FrontAvailable->NextBlock;
		delete FrontAvailable;
		FrontAvailable = Next;
	}
	AvailableBlocks = nullptr;
}

void* FTypedElementDatabaseScratchBuffer::Allocate(size_t Size, size_t Alignment)
{
	// ReaderAccessDetector to assert that this scope has shared access to any thread also making an Allocate call
	UE_MT_SCOPED_READ_ACCESS(AccessDetector);
	return GetThreadLocalBlockController().Allocate(Size, Alignment, FrameId);
}

void FTypedElementDatabaseScratchBuffer::NextFrame()
{
	FrameId++;
}

void FTypedElementDatabaseScratchBuffer::RecycleBlocks()
{
	if (FullBlocks)
	{
		// WriterAccessDetector to assert that this scope has exclusive access - should not overlap Allocate()
		UE_MT_SCOPED_WRITE_ACCESS(AccessDetector);
		
		// Destroy any objects in blocks. It walks the allocations in a block backwards and invokes the destructors on the allocated 
		// objects. It moves to the next block once it reaches the beginning of the block. At the end the cleared blocks are reinserted 
		// into the available blocks for reuse. This approach allows for simple lock-free appending of the destructor chain while remaining
		// relatively simple to destroy. The dirty blocks are added in front of the available blocks instead of the end to avoid having
		// to iterate over the entire list of available blocks.

#if TEDS_SCRATCHBUFFER_ENABLE_ADDITIONAL_TESTS
		// Check to see if there are any cycles.
		{
			TSet<FBlock*> CycleDetector;
			FBlock* TestBlock = FullBlocks;
			while (TestBlock)
			{
				checkf(CycleDetector.Find(TestBlock) == nullptr, TEXT("A circular dependency was found in scratch buffer."));
				CycleDetector.Add(TestBlock);
				TestBlock = TestBlock->NextBlock;
			}
		}
		{
			TSet<FBlock*> CycleDetector;
			FBlock* TestBlock = AvailableBlocks;
			while (TestBlock)
			{
				checkf(CycleDetector.Find(TestBlock) == nullptr, TEXT("A circular dependency was found in scratch buffer."));
				CycleDetector.Add(TestBlock);
				TestBlock = TestBlock->NextBlock;
			}
		}
#endif // TEDS_SCRATCHBUFFER_ENABLE_ADDITIONAL_TESTS

		FBlock* RemainingBlocks = nullptr;
		FBlock* DirtyBlock = FullBlocks;
		do
		{
			// If the block hasn't been touched for at least one frame it can be recycled.
			if (DirtyBlock->LastTouchedByFrame < FrameId)
			{
				// Call destructors on any memory that needs it.
				FDestructorTail* Destructor = DirtyBlock->DestructionTail;
				while (Destructor)
				{
					Destructor->Destructor(reinterpret_cast<char*>(Destructor) - Destructor->StructOffset, Destructor->InstanceCount);
					Destructor = Destructor->PreviousTail;
				}
				DirtyBlock->DestructionTail = nullptr;

				// The block is now clean so set the next block to process.
				FBlock* CleanBlock = DirtyBlock;
				DirtyBlock = DirtyBlock->NextBlock;
				
				// Reinsert the clean block into the chain of available blocks.
				checkf(AvailableBlocks != CleanBlock, TEXT("Recycled block has already been added to the available blocks."));
				CleanBlock->NextBlock.store(AvailableBlocks);
				AvailableBlocks = CleanBlock;
			}
			else
			{
				// The block can't be processed yet so move onto the next block.
				FBlock* DelayedBlock = DirtyBlock;
				DirtyBlock = DirtyBlock->NextBlock;

				// Store the delayed blocks for future processing.
				DelayedBlock->NextBlock = RemainingBlocks;
				RemainingBlocks = DelayedBlock;
			}
		} while (DirtyBlock);
		
		// Set the full blocks to the first block that couldn't be removed or null if all filled up blocks have been recycled.
		FullBlocks = RemainingBlocks;

#if TEDS_SCRATCHBUFFER_ENABLE_ADDITIONAL_TESTS
		// Check if no new cycles have been introduced after updating.
		{
			TSet<FBlock*> CycleDetector;
			FBlock* TestBlock = FullBlocks;
			while (TestBlock)
			{
				checkf(CycleDetector.Find(TestBlock) == nullptr, TEXT("A circular dependency was found in scratch buffer."));
				checkf(TestBlock->LastTouchedByFrame >= FrameId, TEXT("Deleted block still referenced."));
				CycleDetector.Add(TestBlock);
				TestBlock = TestBlock->NextBlock;
			}
		}
		{
			TSet<FBlock*> CycleDetector;
			FBlock* TestBlock = AvailableBlocks;
			while (TestBlock)
			{
				checkf(CycleDetector.Find(TestBlock) == nullptr, TEXT("A circular dependency was found in scratch buffer."));
				CycleDetector.Add(TestBlock);
				TestBlock = TestBlock->NextBlock;
			}
		}
#endif // TEDS_SCRATCHBUFFER_ENABLE_ADDITIONAL_TESTS
	}
}

FTypedElementDatabaseScratchBuffer::FBlockControllerMap& FTypedElementDatabaseScratchBuffer::GetThreadLocalBlockControllerMap()
{
	thread_local static FBlockControllerMap LocalBlockMap;
	return LocalBlockMap;
}

FTypedElementDatabaseScratchBuffer::FBlockController& FTypedElementDatabaseScratchBuffer::GetThreadLocalBlockController()
{
	return GetThreadLocalBlockControllerMap().FindOrAddControllerFor(*this);
}

void FTypedElementDatabaseScratchBuffer::ConfigureDestructorTail(
	FDestructorTail& Destructor, DestructorFunction Callback, void* Object, int32 Count)
{
	GetThreadLocalBlockController().ConfigureDestructorTail(Destructor, Callback, Object, Count);
}


//
// FBlockController
//

FTypedElementDatabaseScratchBuffer::FBlockController::FBlockController(const TWeakPtr<FTypedElementDatabaseScratchBuffer>& InParent)
	: Parent(InParent)
{
	Block = GetEmptyBlock();
}

FTypedElementDatabaseScratchBuffer::FBlockController::~FBlockController()
{
	checkf(Block != nullptr, TEXT("FBlockController is expected to always have a valid block to work with."));
	RecycleBlock();
}

void* FTypedElementDatabaseScratchBuffer::FBlockController::Allocate(size_t Size, size_t Alignment, uint64 LocalFrameId)
{
	checkf(Alignment <= alignof(FBlock), TEXT("Alignment of %i for allocation in database scratch buffer exceeds maximal alignment of %i."),
		static_cast<int>(Alignment), static_cast<int>(alignof(FBlock)));

	if (Size <= MaxAllocationSize())
	{
		size_t Index = Align(Block->Front, Alignment);
		size_t NewFront = Index + Size;

		if (NewFront > FBlock::BlockSize)
		{
			// No more space left in the block so recycle the current one and create a new one.
			RecycleBlock();
			Block = GetEmptyBlock();
			Index = 0;
			NewFront = Size;
		}

		Block->LastTouchedByFrame = LocalFrameId;
		Block->Front = NewFront;
		return Block->Buffer + Index;
	}
	else
	{
		struct FExtendedBufferStore
		{
			void* ExtendedBuffer;
			FDestructorTail Destructor;
		};

		// Allocate memory directly as the requested size doesn't fit in a block.
		void* ExtendedBuffer = FMemory::Malloc(Size, Alignment);
		
		// Add an entry to the block with the sole purpose of deleting the extended buffer.
		void* BufferStoreAddress = Allocate(sizeof(FExtendedBufferStore), alignof(FExtendedBufferStore), LocalFrameId);
		FExtendedBufferStore* BufferStore = reinterpret_cast<FExtendedBufferStore*>(BufferStoreAddress);
		BufferStore->ExtendedBuffer = ExtendedBuffer;

		ConfigureDestructorTail(BufferStore->Destructor,
			[](void* Object, [[maybe_unused]] int32 ObjectCount)
			{
				FMemory::Free(reinterpret_cast<FExtendedBufferStore*>(Object)->ExtendedBuffer);
			}, BufferStore);

		return ExtendedBuffer;
	}
}

FTypedElementDatabaseScratchBuffer::FBlock* FTypedElementDatabaseScratchBuffer::FBlockController::GetEmptyBlock()
{
	if (TSharedPtr<FTypedElementDatabaseScratchBuffer> ParentInstance = Parent.Pin())
	{
		FBlock* FrontBlock = ParentInstance->AvailableBlocks.load();
		// Because the full and available queues are separate, this does not suffer from the ABA problem.
		while (FrontBlock != nullptr && !ParentInstance->AvailableBlocks.compare_exchange_weak(FrontBlock, FrontBlock->NextBlock)){};
		if (FrontBlock)
		{
			FrontBlock->NextBlock = nullptr;
			return FrontBlock;
		}
	}

	// If null then there are no more buffers left so create a new one.
	return new FBlock();
}

void FTypedElementDatabaseScratchBuffer::FBlockController::RecycleBlock()
{
	if (TSharedPtr<FTypedElementDatabaseScratchBuffer> ParentInstance = Parent.Pin())
	{
		Block->Front = 0;
		
		// Replace the front of the linked list with full blocks with the latest full block.
		// Because the full and available queues are separate, this does not suffer from the ABA problem.
		FBlock* FrontBlock = ParentInstance->FullBlocks;
		do
		{
			Block->NextBlock = FrontBlock;
		} while (!ParentInstance->FullBlocks.compare_exchange_strong(FrontBlock, Block));
	}
	else
	{
		// The assumption is that if the scratch buffer has been deleted none of the data in the
		// current buffer will be used so it can be deleted. It also means that the scratch buffer
		// is not going to recycle this block for this thread so needs to be deleted here to avoid
		// a memory leak.
		delete Block;
	}
	Block = nullptr;
}

void FTypedElementDatabaseScratchBuffer::FBlockController::ConfigureDestructorTail(
	FDestructorTail& Destructor, DestructorFunction Callback, void* Object, int32 Count)
{
	Destructor.Destructor = Callback;
	Destructor.PreviousTail = Block->DestructionTail;
	Destructor.StructOffset = static_cast<int32>(reinterpret_cast<char*>(&Destructor) - reinterpret_cast<char*>(Object));
	Destructor.InstanceCount = Count;
	Block->DestructionTail = &Destructor;
}



//
// FBlockControllerMap
//

FTypedElementDatabaseScratchBuffer::FBlockController& 
	FTypedElementDatabaseScratchBuffer::FBlockControllerMap::FindOrAddControllerFor(FTypedElementDatabaseScratchBuffer& ScratchBuffer)
{
	TWeakPtr<FTypedElementDatabaseScratchBuffer>* ParentsIt = Parents.GetData();
	const uint32 Count = Parents.Num();
	for (uint32 Index = 0; Index < Count; ++Index, ++ParentsIt)
	{
		if (ParentsIt->HasSameObject(&ScratchBuffer))
		{
			return Controllers[Index];
		}
	}

	Parents.Add(ScratchBuffer.AsWeak());
	uint32 NewIndex = Controllers.Emplace(ScratchBuffer.AsWeak());
	return Controllers[NewIndex];
}
