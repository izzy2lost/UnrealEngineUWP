// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMVar.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VObject);
TGlobalTrivialEmergentTypePtr<&VObject::StaticCppClassInfo> VObject::GlobalTrivialEmergentType;

template <typename TVisitor>
void VObject::VisitReferencesImpl(TVisitor& Visitor)
{
	for (uint64 Index = GetEmergentType()->Shape->NumIndexedFields; Index--;)
	{
		Visitor.Visit(Data[Index]);
	}
}

VObject& VObject::New(FAllocationContext Context, VClass& InClass, VUniqueStringSet& InFields, const TArray<VValue>& InValues)
{
	/*
	 * We are allocating _all_ fields since for now we're "flattening" the fields in the shape vended. i.e.
	 * c1 := class:
	 *    A:int = 5
	 *    B:float = 0.1
	 *    var C:string
	 *    var D:int = 2
	 *
	 * foo():void=
	 *    tmp:c1 = c1{A:= 2, C:= "test"}
	 *
	 *    <#>
	 *        emergent type/shape for `tmp` archetype instantiation/object is:
	 *        A := 2
	 *        B := 0.1     <- This is copied from `c1`'s shape, rather than linking to `c1`'s shape.
	 *        C := "test"  <- this is still a `var`
	 *        D := 2       <- Still needs object-specific space allocated for it since it can be overridden.
	 */
	// Guarantees we have enough space on the object for all combined fields, in the future when we support extension data members.
	// i.e. adding a new `E` field on the object `tmp.E:int = 5` after the initial archetype construction.
	VEmergentType& NewEmergentType = InClass.GetOrCreateEmergentTypeForArchetype(Context, InFields);

	// At this point, the new emergent type + shape contains all the re-ordered fields that combine the class and (if any)
	// its inherited class fields and values as well.
	VObject& NewObject = VObject::New(Context, NewEmergentType);

	// For fields that are not being overridden and are offset-based, we must grab the default values from the class's
	// shape and copy them to the object so that the object can continue to set them in the future.
	// Otherwise, use the overridden values instead.
	for (auto& Pair : NewEmergentType.Shape->Fields)
	{
		// NOTE: We are assuming that the order of unique strings being added to the set is stable and that the set element
		// IDs correspond to the index at which each unique string was added. This means that the set of unique strings
		// must not have been mutated after creation, as it could potentially re-order the elements!
		const FSetElementId ElementId = InFields.FindId(Pair.Key->AsStringView());
		if (InFields.IsValidId(ElementId))
		{
			switch (Pair.Value.Type)
			{
				case EFieldType::Mutable:
				case EFieldType::Offset:
					NewObject.Data[Pair.Value.Index].Set(Context, InValues[ElementId.AsInteger()]);
					break;
				case EFieldType::Constant:
				default:
					VERSE_UNREACHABLE();
			}
		}
		else
		{
			switch (Pair.Value.Type)
			{
				case EFieldType::Mutable:
				case EFieldType::Offset:
				{
					// The default value should have been copied over from the class already by this point.
					// This could be uninitialized (e.g. `c1 := class { A:int }; T:c1 = c1{}; T.A := 3`).
					// If so, we don't copy the value to the object since it would then interfere with the
					// assumption that the object's value should be a root value.
					VValue PairValue = Pair.Value.Constant.Get();
					if (PairValue)
					{
						// TODO: This should be subsumed by more general support for effectful initializers.
						if (Pair.Value.Type == EFieldType::Mutable)
						{
							VVar& Var = VVar::New(Context);
							Var.SetNonTransactionally(Context, PairValue);
							PairValue = Var;
						}
						NewObject.Data[Pair.Value.Index].Set(Context, PairValue);
					}
					break;
				}
				case EFieldType::Constant:
					break; // Data lives in the shape, so nothing to do here.
				default:
					VERSE_UNREACHABLE();
			}
		}
	}

	return NewObject;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
