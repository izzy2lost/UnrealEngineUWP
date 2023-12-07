// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMHeap.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMMapBase.h"

namespace Verse
{

struct VMutableMap : VMapBase
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VMapBase);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	void InPlaceMakeImmutable(FAllocationContext Context)
	{
		static_assert(std::is_base_of_v<VMapBase, VMap>);
		static_assert(sizeof(VMap) == sizeof(VMapBase));
		SetEmergentType(Context, &VMap::GlobalTrivialEmergentType.Get(Context));
	}

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

	static VMutableMap& New(FAllocationContext Context, std::initializer_list<TPair<VValue, VValue>> InitList)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMutableMap))) VMutableMap(Context, InitList);
	}

	template <typename InitEntryByIndex>
	static VMutableMap& New(FAllocationContext Context, uint32 NumEntries, InitEntryByIndex&& InitEntryFunc)
	{
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMutableMap))) VMutableMap(Context, NumEntries, InitEntryFunc);
	}

	static void SerializeImpl(VMutableMap*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	VMutableMap(FAllocationContext Context, uint32 InitialCapacity)
		: VMapBase(Context, InitialCapacity, &GlobalTrivialEmergentType.Get(Context)) {}

	VMutableMap(FAllocationContext Context, std::initializer_list<TPair<VValue, VValue>> InitList)
		: VMapBase(Context, InitList, &GlobalTrivialEmergentType.Get(Context)) {}

	template <typename InitEntryByIndex>
	VMutableMap(FAllocationContext Context, uint32 NumEntries, InitEntryByIndex&& InitEntryFunc)
		: VMapBase(Context, NumEntries, InitEntryFunc, &GlobalTrivialEmergentType.Get(Context)) {}
};

} // namespace Verse
