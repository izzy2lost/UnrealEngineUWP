// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Templates/SharedPointer.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMType.h"

namespace Verse
{
struct VUniqueString;
struct VProcedure;

// TODO: (yiliang.siew) Need to have enough info so that can do dynamic casts in Verse at runtime.
// TODO: (yiliang.siew) Maybe use this to store the inherited types instead?
struct VTypeClass : VType
{
	static constexpr EVerseTypeTag Tag = EVerseTypeTag::Class;
	static VTypeClass* New(FAllocationContext Context)
	{
		return new (Context.AllocateFastCell(sizeof(VTypeClass))) VTypeClass(Context);
	}
	static bool Equals(const VType& Type)
	{
		return Type.IsA<VTypeClass>();
	}

private:
	explicit VTypeClass(FAllocationContext& Context)
		: VType(Context, Tag)
	{
	}
};

/// This provides a custom comparison that allows us to do pointer-based compares of each unique string set, rather than hash-based comparisons.
struct FEmergentTypesCacheKeyFuncs : TDefaultMapKeyFuncs<TWriteBarrier<VUniqueStringSet>, TWriteBarrier<VEmergentType>, /*bInAllowDuplicateKeys*/ false>
{
public:
	static bool Matches(KeyInitType A, KeyInitType B);
	static bool Matches(KeyInitType A, const VUniqueStringSet& B);
	static uint32 GetKeyHash(KeyInitType Key);
	static uint32 GetKeyHash(const VUniqueStringSet& Key);
};

/// A sequence of fields and blocks in a class body.
/// May represent either a single class, or the flattened combination of a subclass and its superclasses.
struct VConstructor : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	struct VEntry
	{
		/// When non-null, the name of this field. When null, this entry represents a block.
		TWriteBarrier<VUniqueString> Name;

		/// When bDynamic, a VProcedure for a default initializer or block, or nothing for an uninitialized field.
		/// Otherwise, a constant VValue for a default field value (which may be a VProcedure for functions, which bind Self lazily).
		TWriteBarrier<VValue> Value;
		bool bDynamic;

		static VEntry Constant(FAllocationContext Context, VUniqueString& InField, VValue InValue)
		{
			return VEntry{
				{Context, InField},
				{Context, InValue},
				false
            };
		}

		static VEntry Field(FAllocationContext Context, VUniqueString& InField)
		{
			return {
				{Context, InField},
				{},
				true
            };
		}

		static VEntry FieldInitializer(FAllocationContext Context, VUniqueString& InField, VProcedure& Code)
		{
			return {
				{Context,      InField},
				{Context, VValue(Code)},
				true
            };
		}

		static VEntry Block(FAllocationContext Context, VProcedure& Code)
		{
			return {
				{},
				{Context, VValue(Code)},
				true
            };
		}

		VProcedure* Initializer() const
		{
			if (bDynamic && Value.Get())
			{
				return &Value.Get().StaticCast<VProcedure>();
			}
			else
			{
				return nullptr;
			}
		}
	};

	uint32 NumEntries;
	VEntry Entries[];

	static VConstructor& New(FAllocationContext Context, const TArray<VEntry>& InEntries)
	{
		size_t NumBytes = offsetof(VConstructor, Entries) + InEntries.Num() * sizeof(Entries[0]);
		return *new (Context.AllocateFastCell(NumBytes)) VConstructor(Context, InEntries);
	}

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

private:
	VConstructor(FAllocationContext Context, const TArray<VEntry>& InEntries)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumEntries(InEntries.Num())
	{
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			new (&Entries[Index]) VEntry(InEntries[Index]);
		}
	}
};

/// A first-class Verse value representing a class.
struct VClass : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);

	/**
	 * Creates a new class.
	 *
	 * @param InConstructor The sequence of fields and blocks in the class body.
	 * @param InInherited   An array of base classes in order of inheritance.
	 */
	static VClass& New(FAllocationContext Context, VConstructor& InConstructor, const TArray<VClass*>& InInherited);

	/// Vends an emergent type based on requested fields to override in the class archetype instantiation.
	VEmergentType& GetOrCreateEmergentTypeForArchetype(FAllocationContext Context, VUniqueStringSet& ArchetypeFieldNames);

	VConstructor& GetConstructor() { return *Constructor; }

private:
	VClass(FAllocationContext Context, VConstructor& InConstructor, const TArray<VClass*>& InInherited);

	/// Append to `Entries` those elements of `Base` which are not already overridden, indicated by `Fields`.
	static void Extend(TSet<VUniqueString*>& Fields, TArray<VConstructor::VEntry>& Entries, const VConstructor& Base);

	// TODO: (yiliang.siew) This should be a weak map when we can support it in the GC. https://jira.it.epicgames.com/browse/SOL-5312
	/// This is a cache that allows for fast vending of emergent types based on the fields being overridden.
	TMap<TWriteBarrier<VUniqueStringSet>, TWriteBarrier<VEmergentType>, FDefaultSetAllocator, FEmergentTypesCacheKeyFuncs> EmergentTypesCache;

	/// The combined sequence of initializers and blocks in this class and its superclasses, in execution order.
	/// Actual object construction may further override some elements of this sequence.
	TWriteBarrier<VConstructor> Constructor;

	uint32 NumInherited;
	TWriteBarrier<VClass> Inherited[];
};
}; // namespace Verse
