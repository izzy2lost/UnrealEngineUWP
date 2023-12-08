// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMHeap.h"
#include "VerseVM/VVMMapBase.h"

namespace Verse
{

struct VMap : VMapBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VMapBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VMap& New(FAllocationContext Context, uint32 InitialCapacity = 0)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMap))) VMap(Context, InitialCapacity);
	}

	static VMap& New(FAllocationContext Context, std::initializer_list<TPair<VValue, VValue>> InitList)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMap))) VMap(Context, InitList);
	}

	template <typename InitEntryByIndex>
	static VMap& New(FAllocationContext Context, uint32 NumEntries, InitEntryByIndex&& InitEntryFunc)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMap))) VMap(Context, NumEntries, InitEntryFunc);
	}

	static void SerializeImpl(VMap*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	VMap(FAllocationContext Context, uint32 InitialCapacity)
		: VMapBase(Context, InitialCapacity, &GlobalTrivialEmergentType.Get(Context)) {}

	VMap(FAllocationContext Context, std::initializer_list<TPair<VValue, VValue>> InitList)
		: VMapBase(Context, InitList, &GlobalTrivialEmergentType.Get(Context)) {}

	template <typename InitEntryByIndex>
	VMap(FAllocationContext Context, uint32 NumEntries, InitEntryByIndex&& InitEntryFunc)
		: VMapBase(Context, NumEntries, InitEntryFunc, &GlobalTrivialEmergentType.Get(Context)) {}
};

} // namespace Verse
#endif // WITH_VERSE_VM
