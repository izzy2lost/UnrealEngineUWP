// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMap.h"
#include "Async/ExternalMutex.h"
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

DEFINE_DERIVED_VCPPCLASSINFO(VMap);
TGlobalTrivialEmergentTypePtr<&VMap::StaticCppClassInfo> VMap::GlobalTrivialEmergentType;

void VMap::Add(FAllocationContext Context, VValue Key, VValue Value)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	TWriteBarrier<VValue> NewKey(Context, Key);
	TWriteBarrier<VValue> NewValue(Context, Value);

	const size_t PreviousAllocatedSize = GetAllocatedSize();
	InternalMap.Add(NewKey, NewValue);
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
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	for (VMapInternal::TIterator MapIt = InternalMap.CreateIterator(); MapIt; ++MapIt)
	{
		Visitor.Visit(MapIt.Key());
		Visitor.Visit(MapIt.Value());
	}
}

bool VMap::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VMap>())
	{
		return false;
	}

	VMap& OtherMap = Other->StaticCast<VMap>();
	if (InternalMap.Num() != OtherMap.InternalMap.Num())
	{
		return false;
	}
	for (VMapInternal::TConstIterator LhsIt = InternalMap.CreateConstIterator(); LhsIt; ++LhsIt)
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

uint32 VMap::GetTypeHashImpl()
{
	uint32 Result = 0;
	for (VMapInternal::TConstIterator MapIt = InternalMap.CreateConstIterator(); MapIt; ++MapIt)
	{
		::HashCombineFast(Result, ::HashCombineFast(GetTypeHash(MapIt.Key()), GetTypeHash(MapIt.Value())));
	}
	return Result;
}

VMap::~VMap()
{
	FHeap::ReportDeallocatedNativeBytes(GetAllocatedSize());
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
