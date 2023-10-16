// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMTypeCreator.h"
#include "VerseVM/VVMVisitorWrapper.h"

namespace Verse
{
UE::FMutex VClass::Mutex;

DEFINE_VISIT_REFERENCES(VClass)
DEFINE_VCPPCLASSINFO(VClass, VHeapValue, TEXT("Class"));

VFields::FieldsMap VClass::GetCombinedFields(FAllocationContext Context, const VUniqueStringSet& InFieldNames) const
{
	VFields::FieldsMap AllFields{Fields};
	// Based on the chain of inheritance we want later derived classes to override the values of base classes
	// earlier in the inheritance chain, followed by the actual fields being requested to archetype instantiate this class with.
	for (uint32 Index = 0; Index < NumInherited(); ++Index)
	{
		const TWriteBarrier<VClass>& CurrentInherited = Inherited()[Index];
		AllFields.Append(CurrentInherited->Fields);
	}

	for (const TWriteBarrier<VUniqueString>& FieldName : InFieldNames)
	{
		VFields::VEntry* Entry = AllFields.Find(FieldName);
		// If the entry doesn't exist, add it to the map and treat it as an offset-based field
		// in order to support extension data members in the future.
		if (!Entry)
		{
			Entry = &AllFields.Add({Context, VUniqueString::New(Context, FieldName.Get()->AsStringView())}, {Context, {}, EFieldType::Offset});
		}
		Entry->Type = EFieldType::Offset; // Offset here, because just the field names alone won't tell us if a value is being provided.
		Entry->Index = 0;                 // Just zero out the entry first; the re-ordering of indices comes later.
	}

	return AllFields;
}

VEmergentType& VClass::GetOrCreateEmergentTypeForArchetype(FAllocationContext Context, VUniqueStringSet& ArchetypeFieldNames)
{
	UE::TUniqueLock Lock(Mutex);

	// TODO: This in the future shouldn't even require a hash table lookup when we introduce inline caching for this.
	if (TWriteBarrier<VEmergentType>* ExistingEmergentType = EmergentTypesCache.FindByHash(GetTypeHash(ArchetypeFieldNames), ArchetypeFieldNames))
	{
		return *(ExistingEmergentType)->Get();
	}

	// First get a mapping of all fields, including those from the inherited classes, then override these fields with
	// the incoming archetype instantiation requested, and then finally vend a shape/emergent type resulting from that.
	// We don't pass in the values associated with the field names here because by virtue of overriding the field during archetype
	// instantiation, it _must_ be an offset-based field since the object must store the data for the field.
	VFields::FieldsMap AllFields = GetCombinedFields(Context, ArchetypeFieldNames);

	VShape* NewShape = VShape::New(Context, MoveTemp(AllFields));
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
	VHeapValue::VisitReferences(this, Visitor);

	// Mark the inherited classes to ensure that they don't get swept during GC since we want to keep their information
	// around when anything needs to query the class inheritance hierarchy.
	Visitor.Visit(Inherited(), NumInherited());

	// We need both the unique string sets and emergent types that are being cached for fast lookup of emergent types to remain allocated.
	UE::TUniqueLock Lock(Mutex);
	for (auto& Pair : EmergentTypesCache)
	{
		Visitor.Visit(Pair.Key);
		Visitor.Visit(Pair.Value);
	}
}

void VClass::RunDestructorImpl(VCell* ThisCell)
{
	VClass& This = ThisCell->StaticCast<VClass>();
	This.~VClass();
}

} // namespace Verse
#endif // WITH_VERSE_VM
