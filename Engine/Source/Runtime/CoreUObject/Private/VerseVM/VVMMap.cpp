// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMMap.h"
#include "Async/UniqueLock.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

uint32 VMapInternalKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return GetTypeHash(Key);
}

uint32 VMapInternalKeyFuncs::GetKeyHash(VValue Key)
{
	return GetTypeHash(Key);
}

DEFINE_VCPPCLASSINFO(VMap, VHeapValue, TEXT("Map"));
TGlobalTrivialEmergentTypePtr<&VMap::StaticCppClassInfo> VMap::GlobalTrivialEmergentType;

void VMap::Add(const TWriteBarrier<VValue>& Key, const TWriteBarrier<VValue>& Value)
{
	UE::TUniqueLock Lock(MapMutex);
	size_t PreviousAllocatedSize = GetAllocatedSize();
	InternalMap.Add(Key, Value);
	FHeap::ReportAllocatedNativeBytes((GetAllocatedSize() - PreviousAllocatedSize));
}

VValue VMap::Find(const VValue Key)
{
	TWriteBarrier<VValue>* Result = InternalMap.FindByHash(GetTypeHash(Key), Key);
	if (Result)
	{
		return Result->Follow();
	}
	return VValue();
}

void VMap::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	VMap* This = static_cast<VMap*>(ThisCell);
	UE::TUniqueLock Lock(This->MapMutex);

	VHeapValue::MarkReferencedCellsImpl(This, MarkStack);
	for (VMapInternal::TIterator MapIt = This->InternalMap.CreateIterator(); MapIt; ++MapIt)
	{
		MapIt.Key().Mark(MarkStack);
		MapIt.Value().Mark(MarkStack);
	}
}

bool VMap::EqualImpl(FRunningContext Context, VCell* ThisCell, VCell* Other, TFunction<void(VValue, VValue)> HandlePlaceholder)
{
	if (!Other->IsA<VMap>())
	{
		return false;
	}

	VMap& ThisMap = ThisCell->StaticCast<VMap>();
	VMap& OtherMap = Other->StaticCast<VMap>();
	if (ThisMap.InternalMap.Num() != OtherMap.InternalMap.Num())
	{
		return false;
	}
	for (VMapInternal::TConstIterator LhsIt = ThisMap.InternalMap.CreateConstIterator(); LhsIt; ++LhsIt)
	{
		const VValue RhsValue = OtherMap.Find(LhsIt.Key().Get());
		if (!RhsValue)
		{
			return false;
		}

		if (!VValue::Equal(Context, LhsIt.Value().Get(), RhsValue, HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

uint32 VMap::GetTypeHashImpl(VCell* ThisCell)
{
	VMap& This = ThisCell->StaticCast<VMap>();
	uint32 Result = 0;
	for (VMapInternal::TConstIterator MapIt = This.InternalMap.CreateConstIterator(); MapIt; ++MapIt)
	{
		::HashCombineFast(Result, ::HashCombineFast(GetTypeHash(MapIt.Key()), GetTypeHash(MapIt.Value())));
	}
	return Result;
}

void VMap::RunDestructorImpl(VCell* ThisCell)
{
	VMap* This = static_cast<VMap*>(ThisCell);
	FHeap::ReportDeallocatedNativeBytes(This->GetAllocatedSize());
	This->~VMap();
}

} // namespace Verse
#endif // WITH_VERSE_VM