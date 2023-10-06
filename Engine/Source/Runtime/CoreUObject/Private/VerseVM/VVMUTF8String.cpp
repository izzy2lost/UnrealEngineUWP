// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "Async/UniqueLock.h"
#include "Containers/StringView.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{
UE::FMutex VStringInternPool::Mutex;

VUniqueString& VStringInternPool::Intern(FAllocationContext Context, FUtf8StringView String)
{
	UE::TUniqueLock Lock(Mutex);
	if (TWeakBarrier<VUniqueString>* UniqueStringEntry = UniqueStrings.Find(String))
	{
		// If we found an entry, but GC clears the weak reference before we can use it, fall through
		// to add a new entry for the string.
		if (VUniqueString* UniqueString = UniqueStringEntry->Get(Context))
		{
			return *UniqueString;
		}
	}

	VUniqueString& UniqueString = VUniqueString::Make(Context, String);
	UniqueStrings.Add(TWeakBarrier<VUniqueString>(UniqueString));
	return UniqueString;
}

void VStringInternPool::ConductCensus()
{
	UE::TUniqueLock Lock(Mutex);
	for (auto It = UniqueStrings.CreateIterator(); It; ++It)
	{
		// If the cell that the string is allocated in is not marked (i.e. non-live) during GC marking
		// the weak reference will be removed and thus we can remove the map entry from the pool as well.
		if (It->ClearWeakDuringCensus())
		{
			It.RemoveCurrent();
		}
	}
}

uint32 VUTF8String::GetTypeHashImpl(VCell* ThisCell)
{
	VUTF8String& ThisString = ThisCell->StaticCast<VUTF8String>();
	return GetTypeHash(ThisString);
}

DEFINE_VCPPCLASSINFO(VUTF8String, VHeapValue, TEXT("UTF8String"));
TGlobalTrivialEmergentTypePtr<&VUTF8String::StaticCppClassInfo> VUTF8String::GlobalTrivialEmergentType;

DEFINE_VCPPCLASSINFO(VUniqueString, VHeapValue, TEXT("UniqueString"));
TGlobalTrivialEmergentTypePtr<&VUniqueString::StaticCppClassInfo> VUniqueString::GlobalTrivialEmergentType;

TLazyInitialized<VStringInternPool> VUniqueString::StringPool;

VUniqueStringSet& VUniqueStringSetInternPool::Intern(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
{
	UE::TUniqueLock Lock(Mutex);
	if (TWeakBarrier<VUniqueStringSet>* UniqueSet = Sets.Find(InSet))
	{
		// If we found an entry, but GC clears the weak reference before we can use it, fall through
		// to add a new entry for the set.
		if (VUniqueStringSet* CurrentSet = UniqueSet->Get(Context))
		{
			return *CurrentSet;
		}
	}

	VUniqueStringSet& UniqueStringSet = VUniqueStringSet::Make(Context, InSet);
	Sets.Add({UniqueStringSet});
	return UniqueStringSet;
}

void VUniqueStringSetInternPool::ConductCensus()
{
	UE::TUniqueLock Lock(Mutex);
	for (auto It = Sets.CreateIterator(); It; ++It)
	{
		// If the cell that the string is allocated in is not marked (i.e. non-live) during GC marking
		// the weak reference will be removed and thus we can remove the map entry from the pool as well.
		if (It->ClearWeakDuringCensus())
		{
			It.RemoveCurrent();
		}
	}
}

UE::FMutex VUniqueStringSetInternPool::Mutex;

DEFINE_VCPPCLASSINFO(VUniqueStringSet, VCell, TEXT("HashableUniqueStringSet"));
TGlobalTrivialEmergentTypePtr<&VUniqueStringSet::StaticCppClassInfo> VUniqueStringSet::GlobalTrivialEmergentType;

TLazyInitialized<VUniqueStringSetInternPool> VUniqueStringSet::Pool;

} // namespace Verse
#endif // WITH_VERSE_VM
