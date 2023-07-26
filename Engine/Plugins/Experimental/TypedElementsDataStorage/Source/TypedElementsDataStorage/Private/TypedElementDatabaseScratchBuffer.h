// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>
#include <type_traits>
#include "Misc/MTAccessDetector.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/UnrealTemplate.h"

// A thread-safe memory allocator that uses linear allocation using a chain of recycled memory blocks.
// This provides a fast and lightweight way to allocate temporary memory for intermediate values that will
// live at best until the end of the frame. Objects that require destruction will be deleted once a block is
// recycled. A block is only recycled when it's full, so during low activity on a particular thread it can
// take significantly longer than a single frame before an object is destroyed.
class FTypedElementDatabaseScratchBuffer
{
	friend class FBlockController;
public:
	~FTypedElementDatabaseScratchBuffer();

	void* Allocate(size_t Size, size_t Alignment);
	template<typename T, typename... ArgTypes>
	T* Emplace(ArgTypes... Args);
	template<typename T, typename... ArgTypes>
	T* EmplaceArray(int32 Count, const ArgTypes&... Args);

	// Makes previously filled up blocks available again. This can only safely be called when Allocate(...) can not
	// be called. As allocations should only be called when Mass is running processors, it means this function
	// can be safely called when the processors are not running, i.e. at the end of a tick.
	void RecycleBlocks();

	constexpr static int32 MaxAllocationSize();

private:
	using DestructorFunction = void (*)(void* Object, int32 ObjectCount);

	// Track objects that need to be deleted. This will happen when a chain of blocks is recycled. This can
	// mean that an objects stays around for several frames if a block isn't heavily used.
	struct FDestructorTail
	{
		FDestructorTail* PreviousTail;
		DestructorFunction Destructor;
		int32 StructOffset;
		int32 InstanceCount;
	};

	// Use the largest SIMD alignment for current hardware just in case. The buffer is sufficiently large
	// that the overhead won't matter.
	struct alignas(64) FBlock
	{
		constexpr static size_t BlockSize = 64 * 1024; // 64kb blocks
		
		char Buffer[BlockSize]; // Keep at top for alignment.
		FDestructorTail* DestructionTail = nullptr;
		std::atomic<FBlock*> NextBlock = nullptr;
		std::atomic<uint32> Owner = 0;
		// Offset into the buffer where the next allocation starts.
		uint32 Front = 0;
	};

	// Simple wrapper around the buffer. This is to guarantee that when the thread gets destroyed the buffer is cleaned up.
	struct FBlockController
	{
		explicit FBlockController(FTypedElementDatabaseScratchBuffer& InOwner);
		~FBlockController();

		void* Allocate(size_t Size, size_t Alignment);
		FBlock* GetEmptyBlock();
		void RecycleBlock();
		void ConfigureDestructorTail(FDestructorTail& Destructor, DestructorFunction Callback, void* Object, int32 Count = 1);

		FTypedElementDatabaseScratchBuffer& Owner;
		FBlock* Block;
		uint32 Id;
	};

	// Returns the block controller that's unique to the thread that calls this function.
	FBlockController& GetThreadLocalBlockController();
	void ConfigureDestructorTail(FDestructorTail& Destructor, DestructorFunction Callback, void* Object, int32 Count = 1);

	// Detect any invalid calls to recycle blocks while allocating memory.
	FRWRecursiveAccessDetector AccessDetector;

	std::atomic<FBlock*> AvailableBlocks = nullptr;
	std::atomic<FBlock*> FullBlocks = nullptr;
	// Running counter so each FBlockController gets a unique id to identify the blocks assigned to them. Zero is reserved 
	// to indicate it's not in use by a block controller.
	std::atomic<uint32> BlockControllerId = 1;
};


//
// Implementations
//

template<typename T, typename... ArgTypes>
T* FTypedElementDatabaseScratchBuffer::Emplace(ArgTypes... Args)
{
	if constexpr (std::is_trivially_destructible_v<T>)
	{
		return new(Allocate(sizeof(T), alignof(T))) T(Forward<ArgTypes>(Args)...);
	}
	else
	{
		struct Wrapper
		{
			T Instance;
			FDestructorTail Destructor;
		};
		Wrapper* Result = reinterpret_cast<Wrapper*>(Allocate(sizeof(Wrapper), alignof(Wrapper)));
		new(&Result->Instance) T(Forward<ArgTypes>(Args)...);

		ConfigureDestructorTail(Result->Destructor, 
			[](void* Object, [[maybe_unused]] int32 ObjectCount)
			{ 
				reinterpret_cast<T*>(Object)->~T(); 
			}, &Result->Instance);

		return &Result->Instance;
	}
}

template<typename T, typename... ArgTypes>
T* FTypedElementDatabaseScratchBuffer::EmplaceArray(int32 Count, const ArgTypes&... Args)
{
	T* Result;	
	if constexpr (std::is_trivially_destructible_v<T>)
	{
		Result = reinterpret_cast<T*>(Allocate(sizeof(T) * Count, alignof(T)));
	}
	else
	{
		constexpr size_t Alignment = std::max(alignof(T), alignof(FDestructorTail));
		int32 DestructorTailOffset = Align(sizeof(T) * Count, alignof(FDestructorTail));
		int32 Size = DestructorTailOffset + sizeof(FDestructorTail);
		Result = reinterpret_cast<T*>(Allocate(Size, Alignment));

		FDestructorTail* Destructor = reinterpret_cast<FDestructorTail*>(reinterpret_cast<char*>(Result) + DestructorTailOffset);
		ConfigureDestructorTail(*Destructor,
			[](void* Object, int32 ObjectCount)
			{
				T* Array = reinterpret_cast<T*>(Object);
				for (int32 Count = 0; Count < ObjectCount; ++Count)
				{
					Array->~T();
					++Array;
				}
			}, Result, Count);
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		new(Result + Index) T(Args...);
	}

	return Result;
}

constexpr int32 FTypedElementDatabaseScratchBuffer::MaxAllocationSize()
{
	// Leave room for at least one destructor tail in case a single large object is added that requires destruction.
	return FBlock::BlockSize - sizeof(FDestructorTail);
}
