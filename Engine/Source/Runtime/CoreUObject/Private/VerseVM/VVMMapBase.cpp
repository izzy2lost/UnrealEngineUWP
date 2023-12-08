// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMapBase.h"
#include "Async/ExternalMutex.h"
#include "Async/UniqueLock.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMNativeAllocationGuard.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

uint32 VMapBaseInternalKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return GetTypeHash(Key);
}

uint32 VMapBaseInternalKeyFuncs::GetKeyHash(VValue Key)
{
	return GetTypeHash(Key);
}

DEFINE_DERIVED_VCPPCLASSINFO(VMapBase);
TGlobalTrivialEmergentTypePtr<&VMapBase::StaticCppClassInfo> VMapBase::GlobalTrivialEmergentType;

void VMapBase::Add(FAllocationContext Context, VValue Key, VValue Value)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	TWriteBarrier<VValue> NewKey(Context, Key);
	TWriteBarrier<VValue> NewValue(Context, Value);

	TNativeAllocationGuard NativeAllocationGuard(this);
	InternalMap.Add(NewKey, NewValue);
}

// TODO: Using the empty value to indicate not found
// won't work if we have a map of [t]void and use VValue()
// to represent void.
VValue VMapBase::Find(const VValue Key)
{
	TWriteBarrier<VValue>* Result = InternalMap.FindByHash(GetTypeHash(Key), Key);
	if (Result)
	{
		return Result->Follow();
	}
	return VValue();
}

VValue VMapBase::GetKey(const int32 Index)
{
	// Only works as long as nothing is removed from map
	FSetElementId Id = FSetElementId::FromInteger(Index);
	TWriteBarrier<VValue>& Result = InternalMap.Get(Id).Get<0>();
	return Result.Follow();
}

VValue VMapBase::GetValue(const int32 Index)
{
	// Only works as long as nothing is removed from map
	FSetElementId Id = FSetElementId::FromInteger(Index);
	TWriteBarrier<VValue>& Result = InternalMap.Get(Id).Get<1>();
	return Result.Follow();
}

template <typename TVisitor>
void VMapBase::VisitReferencesImpl(TVisitor& Visitor)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	Visitor.Visit(InternalMap, TEXT("Values"));

	Visitor.ReportNativeBytes(GetAllocatedSize());
}

bool VMapBase::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VMapBase>())
	{
		return false;
	}

	VMapBase& OtherMap = Other->StaticCast<VMapBase>();
	if (InternalMap.Num() != OtherMap.InternalMap.Num())
	{
		return false;
	}
	// TODO: This should be an ordered compare
	for (VMapBaseInternal::TConstIterator LhsIt = InternalMap.CreateConstIterator(); LhsIt; ++LhsIt)
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

uint32 VMapBase::GetTypeHashImpl()
{
	uint32 Result = 0;
	for (VMapBaseInternal::TConstIterator MapIt = InternalMap.CreateConstIterator(); MapIt; ++MapIt)
	{
		::HashCombineFast(Result, ::HashCombineFast(GetTypeHash(MapIt.Key()), GetTypeHash(MapIt.Value())));
	}
	return Result;
}

VMapBase::~VMapBase()
{
	FHeap::ReportDeallocatedNativeBytes(GetAllocatedSize());
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
