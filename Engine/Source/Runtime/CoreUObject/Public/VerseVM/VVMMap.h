// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMHeap.h"
#include "VerseVM/Inline/VVMMapBaseInline.h"
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

	template <typename GetEntryByIndex>
	static VMap& New(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMap))) VMap(Context, MaxNumEntries, GetEntry);
	}

	static void SerializeImpl(VMap*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	VMap(FAllocationContext Context, uint32 InitialCapacity)
		: VMapBase(Context, InitialCapacity, &GlobalTrivialEmergentType.Get(Context)) {}

	template <typename GetEntryByIndex>
	VMap(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry)
		: VMapBase(Context, MaxNumEntries, GetEntry, &GlobalTrivialEmergentType.Get(Context)) {}
};

} // namespace Verse
#endif // WITH_VERSE_VM
