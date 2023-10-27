// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMTypeCreator.h"

namespace Verse
{
struct VShape;

inline bool FEmergentTypesCacheKeyFuncs::Matches(FEmergentTypesCacheKeyFuncs::KeyInitType A, FEmergentTypesCacheKeyFuncs::KeyInitType B)
{
	return A == B;
}

inline bool FEmergentTypesCacheKeyFuncs::Matches(FEmergentTypesCacheKeyFuncs::KeyInitType A, const VUniqueStringSet& B)
{
	return *(A.Get()) == B;
}

inline uint32 FEmergentTypesCacheKeyFuncs::GetKeyHash(FEmergentTypesCacheKeyFuncs::KeyInitType Key)
{
	return GetTypeHash(Key);
}

inline uint32 FEmergentTypesCacheKeyFuncs::GetKeyHash(const VUniqueStringSet& Key)
{
	return GetTypeHash(Key);
}

inline VClass& VClass::New(FAllocationContext Context, const TArray<VClass*>& InInherited, VFields::FieldsMap&& InFields, VProcedure* Blocks)
{
	const size_t Size = AllocationSize(InInherited.Num());
	return *new (Context.Allocate(FHeap::DestructorSpace, Size)) VClass(Context, InInherited, MoveTemp(InFields), Blocks);
}

inline uint32 VClass::NumInherited() const
{
	return NumInheritedClasses;
}

inline VClass::VClass(FAllocationContext Context, const TArray<VClass*>& InInherited, VFields::FieldsMap&& InFields, VProcedure* InBlocks)
	: VHeapValue(Context, VEmergentTypeCreator::GetOrCreate(Context, VTypeCreator::GetOrCreate<VTypeClass>(Context), &StaticCppClassInfo))
	, Fields([&InFields, &InInherited]() {
		// Based on the chain of inheritance we want later derived classes to override the values of base classes
		// earlier in the inheritance chain, followed by the actual fields being requested to archetype instantiate this class with.
		VFields::FieldsMap Result;
		Result.Reserve(InFields.Num() + InInherited.Num()); // Not the greatest way to predict the size needed.
		for (const VClass* Inherited : InInherited)
		{
			Result.Append(Inherited->Fields);
		}
		Result.Append(InFields);
		return Result;
	}()) // Note that the offset-based fields are _not_ re-ordered yet at this point!
	, Blocks(Context, InBlocks)
	, NumInheritedClasses(InInherited.Num())
{
	uint32 Index = 0;
	for (VClass* CurrentInherited : InInherited)
	{
		// Also record in the current class the inherited classes.
		Inherited()[Index++].Set(Context, *CurrentInherited);
	}
}

inline size_t VClass::DataOffset()
{
	return Align(sizeof(VClass), alignof(TWriteBarrier<VClass>));
}

inline size_t VClass::AllocationSize(const uint32 NumInherited)
{
	return DataOffset() + (NumInherited * sizeof(TWriteBarrier<VClass>));
}

inline TWriteBarrier<VClass>* VClass::Inherited() const
{
	return BitCast<TWriteBarrier<VClass>*>(BitCast<char*>(this) + DataOffset());
}

} // namespace Verse
