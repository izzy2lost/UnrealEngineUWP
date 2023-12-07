// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMVar.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VObject);
TGlobalTrivialEmergentTypePtr<&VObject::StaticCppClassInfo> VObject::GlobalTrivialEmergentType;

template <typename TVisitor>
void VObject::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumIndexedFields = GetEmergentType()->Shape->NumIndexedFields;
		Visitor.BeginArray(TEXT("Data"), ScratchNumIndexedFields);
		Visitor.Visit(Data, Data + ScratchNumIndexedFields);
		Visitor.EndArray();
	}
	else
	{
		Visitor.Visit(Data, Data + GetEmergentType()->Shape->NumIndexedFields);
	}
}

VObject& VObject::New(
	FAllocationContext Context,
	VClass& InClass,
	VUniqueStringSet& InFields,
	const TArray<VValue>& InValues,
	TArray<VProcedure*>& Initializers)
{
	// Combine the class and archetype to determine which fields will live in the object.
	VEmergentType& NewEmergentType = InClass.GetOrCreateEmergentTypeForArchetype(Context, InFields);
	VObject& NewObject = VObject::New(Context, NewEmergentType);

	// Initialize fields from the archetype.
	// NOTE: This assumes that the order of values matches the IDs of the field set.
	for (auto It = InFields.begin(); It != InFields.end(); ++It)
	{
		NewObject.SetField(Context, *It->Get(), InValues[It.GetId().AsInteger()]);
	}

	// Build the sequence of VProcedures to finish object construction.
	VConstructor& Constructor = InClass.GetConstructor();
	V_DIE_UNLESS(Initializers.IsEmpty());
	Initializers.Reserve(Constructor.NumEntries);
	for (uint32 Index = 0; Index < Constructor.NumEntries; ++Index)
	{
		VConstructor::VEntry& Entry = Constructor.Entries[Index];

		// Skip fields which were already initialized above by the archetype.
		if (const VUniqueString* Field = Entry.Name.Get())
		{
			FSetElementId ElementId = InFields.FindId(Field->AsStringView());
			if (InFields.IsValidId(ElementId))
			{
				continue;
			}
		}

		// Record procedures for default initializers and blocks.
		if (VProcedure* Initializer = Entry.Initializer())
		{
			Initializers.Add(Initializer);
		}
	}

	return NewObject;
}

bool VObject::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	// TODO: Should be different for structs that are comparable.
	return this == Other;
}

uint32 VObject::GetTypeHashImpl()
{
	// TODO: Should be different for structs that are comparable.
	return PointerHash(this);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
