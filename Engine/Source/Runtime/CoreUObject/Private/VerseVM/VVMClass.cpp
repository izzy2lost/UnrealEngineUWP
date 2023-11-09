// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "Async/ExternalMutex.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMTypeCreator.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VConstructor);
TGlobalTrivialEmergentTypePtr<&VConstructor::StaticCppClassInfo> VConstructor::GlobalTrivialEmergentType;

template <typename TVisitor>
void VConstructor::VisitReferencesImpl(TVisitor& Visitor)
{
	for (uint32 Index = 0; Index < NumEntries; ++Index)
	{
		Visitor.Visit(Entries[Index].Name);
		Visitor.Visit(Entries[Index].Value);
	}
}

DEFINE_DERIVED_VCPPCLASSINFO(VClass);

void VClass::Extend(TSet<VUniqueString*>& Fields, TArray<VConstructor::VEntry>& Entries, const VConstructor& Base)
{
	for (uint32 Index = 0; Index < Base.NumEntries; ++Index)
	{
		const VConstructor::VEntry& Entry = Base.Entries[Index];
		if (VUniqueString* Field = Entry.Name.Get())
		{
			bool bIsAlreadyInSet;
			Fields.FindOrAdd(Field, &bIsAlreadyInSet);
			if (bIsAlreadyInSet)
			{
				continue;
			}
		}
		Entries.Add(Entry);
	}
}

VEmergentType& VClass::GetOrCreateEmergentTypeForArchetype(FAllocationContext Context, VUniqueStringSet& ArchetypeFieldNames)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	// TODO: This in the future shouldn't even require a hash table lookup when we introduce inline caching for this.
	if (TWriteBarrier<VEmergentType>* ExistingEmergentType = EmergentTypesCache.FindByHash(GetTypeHash(ArchetypeFieldNames), ArchetypeFieldNames))
	{
		return *ExistingEmergentType->Get();
	}

	// Build a combined map of all fields from the archetype, this class, and superclasses.
	// Earlier fields (from the archetype and subclasses) override later fields via `FindOrAdd`.
	VShape::FieldsMap Fields;
	for (const TWriteBarrier<VUniqueString>& Field : ArchetypeFieldNames)
	{
		// Always store fields from the archetype in the object.
		Fields.Add({Context, Field.Get()}, VShape::VEntry::Offset());
	}
	for (uint32 Index = 0; Index < Constructor->NumEntries; ++Index)
	{
		VConstructor::VEntry& Entry = Constructor->Entries[Index];
		if (VUniqueString* Field = Entry.Name.Get())
		{
			if (Entry.bDynamic)
			{
				// Store dynamically-initialized and uninitialized fields in the object.
				Fields.FindOrAdd({Context, Entry.Name.Get()}, VShape::VEntry::Offset());
			}
			else
			{
				// Store constant-initialized fields in the shape.
				Fields.FindOrAdd({Context, Entry.Name.Get()}, VShape::VEntry::Constant(Context, Entry.Value.Get()));
			}
		}
	}

	// Compute the shape by interning the set of fields.
	VShape* NewShape = VShape::New(Context, MoveTemp(Fields));
	VEmergentType* NewEmergentType = VEmergentTypeCreator::GetOrCreate(Context, NewShape, VObject::GlobalTrivialEmergentType.Get(Context).Type.Get(), &VObject::StaticCppClassInfo);
	V_DIE_IF(NewEmergentType == nullptr);

	// This new type will then be kept alive in the cache to re-vend if ever the exact same set of fields are used for
	// archetype instantiation of a different object.
	EmergentTypesCache.Add({Context, ArchetypeFieldNames}, {Context, *NewEmergentType});

	return *NewEmergentType;
}

template <typename TVisitor>
void VClass::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Constructor);

	// Mark the inherited classes to ensure that they don't get swept during GC since we want to keep their information
	// around when anything needs to query the class inheritance hierarchy.
	Visitor.Visit(Inherited, NumInherited);

	// We need both the unique string sets and emergent types that are being cached for fast lookup of emergent types to remain allocated.
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	for (auto& Pair : EmergentTypesCache)
	{
		Visitor.Visit(Pair.Key);
		Visitor.Visit(Pair.Value);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
