// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseScratchBuffer.h"

FTypedElementDatabaseScratchBuffer::~FTypedElementDatabaseScratchBuffer()
{
	// This assumes it's being called after all threads using the scratch buffer have been shutdown already so the
	// scratch buffers they hold have been released back into the pool.

	RecycleBlocks();
	FBlock* FrontFull = FullBlocks;
	while (FrontFull)
	{
		FBlock* Next = FrontFull->NextBlock;
		delete FrontFull;
		FrontFull = Next;
	}
	FullBlocks = nullptr;

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
	TScopedWriterDetector Detector(AccessDetector);
	return GetThreadLocalBlockController().Allocate(Size, Alignment);
}

void FTypedElementDatabaseScratchBuffer::RecycleBlocks()
{
	if (FullBlocks)
	{
		TScopedWriterDetector Detector(AccessDetector);
		
		// Destroy any objects in blocks. It walks the allocations in a block backwards and invokes the destructors on the allocated 
		// objects. It moves to the next block once it reaches the beginning of the block. At the end the cleared blocks are reinserted 
		// into the available blocks for reuse. This approach allows for simple lock-free appending of the destructor chain while remaining
		// relatively simple to destroy. The dirty blocks are added in front of the available blocks instead of the end to avoid having
		// to iterate over the entire list of available blocks.

		FBlock* DirtyBlock = FullBlocks;
		FBlock* LastDirtyBlock = FullBlocks;
		do
		{
			FDestructorTail* Destructor = DirtyBlock->DestructionTail;
			while (Destructor)
			{
				Destructor->Destructor(reinterpret_cast<char*>(Destructor) - Destructor->StructOffset, Destructor->InstanceCount);
				Destructor = Destructor->PreviousTail;
			}
			DirtyBlock->DestructionTail = nullptr;

			LastDirtyBlock = DirtyBlock;
			DirtyBlock = DirtyBlock->NextBlock;
		} while (DirtyBlock);
		
		// Attach the full blocks in front of the available blocks.
		LastDirtyBlock->NextBlock.store(AvailableBlocks);
		AvailableBlocks.store(FullBlocks);
		FullBlocks = nullptr;
	}
}

FTypedElementDatabaseScratchBuffer::FBlockController& FTypedElementDatabaseScratchBuffer::GetThreadLocalBlockController()
{
	thread_local static FBlockController LocalBlock = FBlockController(*this);
	return LocalBlock;
}

void FTypedElementDatabaseScratchBuffer::ConfigureDestructorTail(
	FDestructorTail& Destructor, DestructorFunction Callback, void* Object, int32 Count)
{
	GetThreadLocalBlockController().ConfigureDestructorTail(Destructor, Callback, Object, Count);
}


//
// FBlockController
//

FTypedElementDatabaseScratchBuffer::FBlockController::FBlockController(FTypedElementDatabaseScratchBuffer& InOwner)
	: Owner(InOwner)
	, Id(InOwner.BlockControllerId++)
{
	Block = GetEmptyBlock();
}

FTypedElementDatabaseScratchBuffer::FBlockController::~FBlockController()
{
	RecycleBlock();
}

void* FTypedElementDatabaseScratchBuffer::FBlockController::Allocate(size_t Size, size_t Alignment)
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
		void* BufferStoreAddress = Allocate(sizeof(FExtendedBufferStore), alignof(FExtendedBufferStore));
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
	FBlock* FrontBlock = Owner.AvailableBlocks;
	while (FrontBlock != nullptr)
	{
		// If this is not zero it means another thread has already claimed this block so try again with the next block.
		uint32 CurrentOwner = 0;
		if (FrontBlock->Owner.compare_exchange_strong(CurrentOwner, Id))
		{
			Owner.AvailableBlocks.store(FrontBlock->NextBlock.load());
			FrontBlock->NextBlock = nullptr;
			return FrontBlock;
		}
		else
		{
			// Get the new front block. It can happen due to the thread timing that the same block is retrieved, but 
			// avoiding this adds a lot more complexity and contention on the front block is low.
			FrontBlock = Owner.AvailableBlocks;
		}

	};
	
	// If null then there are no more buffers left so create a new one.
	FBlock* NewBlock = new FBlock();
	NewBlock->Owner = Id;
	return NewBlock;
}

void FTypedElementDatabaseScratchBuffer::FBlockController::RecycleBlock()
{
	Block->Front = 0;
	Block->Owner = 0;

	// Replace the front of the linked list with full blocks with the latest full block.
	FBlock* FrontBlock = Owner.FullBlocks;
	do
	{
		Block->NextBlock = FrontBlock;
	} while (!Owner.FullBlocks.compare_exchange_strong(FrontBlock, Block));
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
