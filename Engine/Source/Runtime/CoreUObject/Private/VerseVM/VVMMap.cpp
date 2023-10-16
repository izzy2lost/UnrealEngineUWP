// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/VVMMap.h"
#include "Async/UniqueLock.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMVisitorWrapper.h"
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

DEFINE_VISIT_REFERENCES(VMap);
DEFINE_VCPPCLASSINFO(VMap, VHeapValue, TEXT("Map"));
TGlobalTrivialEmergentTypePtr<&VMap::StaticCppClassInfo> VMap::GlobalTrivialEmergentType;

void VMap::Add(const TWriteBarrier<VValue>& Key, const TWriteBarrier<VValue>& Value)
{
	UE::TUniqueLock Lock(MapMutex);
	const size_t PreviousAllocatedSize = GetAllocatedSize();
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

VValue VMap::GetKey(const int32 Index)
{
	// Only works as long as nothing is removed from map
	FSetElementId Id = FSetElementId::FromInteger(Index);
	TWriteBarrier<VValue>& Result = InternalMap.Get(Id).Get<0>();
	return Result.Follow();
}

VValue VMap::GetValue(const int32 Index)
{
	// Only works as long as nothing is removed from map
	FSetElementId Id = FSetElementId::FromInteger(Index);
	TWriteBarrier<VValue>& Result = InternalMap.Get(Id).Get<1>();
	return Result.Follow();
}

template <typename TVisitor>
void VMap::VisitReferencesImpl(TVisitor& Visitor)
{
	VHeapValue::VisitReferences(this, Visitor);

	UE::TUniqueLock Lock(MapMutex);

	for (VMapInternal::TIterator MapIt = InternalMap.CreateIterator(); MapIt; ++MapIt)
	{
		Visitor.Visit(MapIt.Key());
		Visitor.Visit(MapIt.Value());
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
	VMap& This = ThisCell->StaticCast<VMap>();
	FHeap::ReportDeallocatedNativeBytes(This.GetAllocatedSize());
	This.~VMap();
}

} // namespace Verse
#endif // WITH_VERSE_VM
