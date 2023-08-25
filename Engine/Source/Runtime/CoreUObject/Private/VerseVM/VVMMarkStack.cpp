// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMMarkStack.h"
#include "VerseVM/VVMLog.h"

namespace Verse
{

FMarkStack::~FMarkStack()
{
	V_DIE_UNLESS(Stack.IsEmpty());
}

template <std::memory_order MemoryOrder>
void FMarkStack::MarkSlowImpl(const VCell* Cell)
{
	std::atomic<uint32>* Word = FHeap::GetMarkBitWord(Cell);
	uint32 Mask = FHeap::GetMarkBitMask(Cell);
	if (!(Word->fetch_or(Mask, MemoryOrder) & Mask))
	{
		Stack.Push(const_cast<VCell*>(Cell));
	}
}

void FMarkStack::MarkSlow(const VCell* Cell)
{
	MarkSlowImpl<std::memory_order_relaxed>(Cell);
}

void FMarkStack::FencedMarkSlow(const VCell* Cell)
{
	MarkSlowImpl<std::memory_order_seq_cst>(Cell);
}

} // namespace Verse
#endif // WITH_VERSE_VM