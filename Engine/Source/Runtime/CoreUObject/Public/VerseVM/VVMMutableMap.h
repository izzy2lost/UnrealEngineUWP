// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMHeap.h"
#include "VerseVM/Inline/VVMMapBaseInline.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMMapBase.h"

namespace Verse
{

struct VMutableMap : VMapBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VMapBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	// TODO: When constructing a map as the result of a for loop, figure out what
	// to do with duplicate keys. Or can we even have duplicate keys in such a scenario?
	void Add(FAllocationContext Context, VValue Key, VValue Value)
	{
		Super::Add(Context, Key, Value);
	}

	static VMutableMap& New(FAllocationContext Context, uint32 InitialCapacity = 0)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMutableMap))) VMutableMap(Context, InitialCapacity);
	}

	template <typename GetEntryByIndex>
	static VMutableMap& New(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMutableMap))) VMutableMap(Context, MaxNumEntries, GetEntry);
	}

	static void SerializeImpl(VMutableMap*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	VMutableMap(FAllocationContext Context, uint32 InitialCapacity)
		: VMapBase(Context, InitialCapacity, &GlobalTrivialEmergentType.Get(Context)) {}

	template <typename GetEntryByIndex>
	VMutableMap(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry)
		: VMapBase(Context, MaxNumEntries, GetEntry, &GlobalTrivialEmergentType.Get(Context)) {}
};

} // namespace Verse
#endif // WITH_VERSE_VM
