// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Templates/SharedPointer.h"
#include "VerseVM/VVMCppClassInfo.h"
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

/// Classes are first class values in Verse, so this inherits from `VHeapValue` rather than `VCell`.
/// `VHeapValue` more represents things that actual values in Verse programs can be, whereas `VCell` is more
/// for things that are internal data values to the VM itself.
struct VClass : VHeapValue
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	DECLARE_VISIT_REFERENCES(COREUOBJECT_API);

	/**
	 * Creates a new class.
	 *
	 * @param InInherited 	This should be an array of the other classes, in order of inheritance, that this class inherits from.
	 * @param InFields 	This should contain the field names and default values (if any), along with the attributes of each entry.
	 */
	static VClass& New(FAllocationContext Context, const TArray<VClass*>& InInherited, VFields::FieldsMap&& InFields, VProcedure* Blocks);

	uint32 NumInherited() const;

	/// Vends an emergent type based on requested fields to override in the class archetype instantiation.
	VEmergentType& GetOrCreateEmergentTypeForArchetype(FAllocationContext Context, VUniqueStringSet& ArchetypeFieldNames);

	VProcedure* GetBlocks() { return Blocks.Get(); }

private:
	VClass(FAllocationContext Context, const TArray<VClass*>& InInherited, VFields::FieldsMap&& InFields, VProcedure* Blocks);
	~VClass() = default;

	/// Gets the combined fields (i.e. including inherited classes) and values. Can also specify additional fields
	/// to override existing fields with; the result will have re-ordered indices for offset-based fields.
	VFields::FieldsMap GetCombinedFields(FAllocationContext Context, const VUniqueStringSet& InFieldNames) const;

	static void RunDestructorImpl(VCell* This);

	static size_t DataOffset();
	static size_t AllocationSize(const uint32 NumInherited);

	/// Each class is guarded with a write barrier because we need to be able to mark the inherited classes within this class as still live.
	TWriteBarrier<VClass>* Inherited() const;

	/// This class's fields and default values (if any). This also includes the inherited classes' fields/values.
	const VFields::FieldsMap Fields;

	/// A procedure for the collection of blocks in the class body.
	TWriteBarrier<VProcedure> Blocks;

	// TODO: (yiliang.siew) This should be a weak map when we can support it in the GC. https://jira.it.epicgames.com/browse/SOL-5312
	/// This is a cache that allows for fast vending of emergent types based on the fields being overridden.
	TMap<TWriteBarrier<VUniqueStringSet>, TWriteBarrier<VEmergentType>, FDefaultSetAllocator, FEmergentTypesCacheKeyFuncs> EmergentTypesCache;

	const uint32 NumInheritedClasses;
};
}; // namespace Verse
