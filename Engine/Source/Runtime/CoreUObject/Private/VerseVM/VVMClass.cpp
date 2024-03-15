// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMClass.h"
#include "Async/ExternalMutex.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/VerseValueProperty.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMTypeCreator.h"
#include "VerseVM/VVMUClass.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VConstructor);
TGlobalTrivialEmergentTypePtr<&VConstructor::StaticCppClassInfo> VConstructor::GlobalTrivialEmergentType;

void VConstructor::SerializeImpl(VConstructor*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint64 ScratchNumEntries = 0;
		Visitor.BeginArray(TEXT("Entries"), ScratchNumEntries);
		This = &VConstructor::NewUninitialized(Context, (uint32)ScratchNumEntries);
		for (uint32 Index = 0; Index < This->NumEntries; ++Index)
		{
			Visitor.BeginObject();
			Visitor.Visit(This->Entries[Index].Name, TEXT("Name"));
			Visitor.Visit(This->Entries[Index].Value, TEXT("Value"));
			Visitor.Visit(This->Entries[Index].bDynamic, TEXT("Dynamic"));
			Visitor.EndObject();
		}
		Visitor.EndArray();
	}
	else
	{
		This->VisitReferences(Visitor);
	}
}

template <typename TVisitor>
void VConstructor::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumEntries = NumEntries;
		Visitor.BeginArray(TEXT("Entries"), ScratchNumEntries);
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			Visitor.BeginObject();
			Visitor.Visit(Entries[Index].Name, TEXT("Name"));
			Visitor.Visit(Entries[Index].Value, TEXT("Value"));
			Visitor.Visit(Entries[Index].bDynamic, TEXT("Dynamic"));
			Visitor.EndObject();
		}
		Visitor.EndArray();
	}
	else
	{
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			Visitor.Visit(Entries[Index].Name, TEXT("Name"));
			Visitor.Visit(Entries[Index].Value, TEXT("Value"));
		}
	}
}

void VConstructor::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	Builder.Append(TEXT("\n"));
	for (uint32 Index = 0; Index < NumEntries; ++Index)
	{
		const VEntry& Entry = Entries[Index];
		Builder.Append(TEXT("\t"));
		Formatter.Append(Builder, Context, *Entry.Name);
		Builder.Append(TEXT(" : Entry(Value: "));
		Entry.Value.Get().ToString(Builder, Context, Formatter);
		Builder.Append(TEXT(", Dynamic: "));
		Builder.Append(Entry.bDynamic ? TEXT("true") : TEXT("false"));
		Builder.Append(TEXT("))\n"));
	}
}

DEFINE_DERIVED_VCPPCLASSINFO(VClass)
TGlobalTrivialEmergentTypePtr<&VClass::StaticCppClassInfo> VClass::GlobalTrivialEmergentType;

template <typename TVisitor>
void VClass::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(ClassName, TEXT("ClassName"));
	Visitor.Visit(UEMangledName, TEXT("UEMangledName"));
	Visitor.Visit(Scope, TEXT("Scope"));
	Visitor.Visit(Constructor, TEXT("Constructor"));
	Visitor.Visit(AssociatedUClass, TEXT("AssociatedUClass"));

	// Mark the inherited classes to ensure that they don't get swept during GC since we want to keep their information
	// around when anything needs to query the class inheritance hierarchy.
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumInherited = NumInherited;
		Visitor.BeginArray(TEXT("Inherited"), ScratchNumInherited);
		Visitor.Visit(Inherited, Inherited + NumInherited);
		Visitor.EndArray();
	}
	else
	{
		Visitor.Visit(Inherited, Inherited + NumInherited);
	}

	// We need both the unique string sets and emergent types that are being cached for fast lookup of emergent types to remain allocated.
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);
	Visitor.Visit(EmergentTypesCache, TEXT("EmergentTypesCache"));
}

void VClass::Extend(TSet<VUniqueString*>& Fields, TArray<VConstructor::VEntry>& Entries, const VConstructor& Base)
{
	for (uint32 Index = 0; Index < Base.NumEntries; ++Index)
	{
		const VConstructor::VEntry& Entry = Base.Entries[Index];
		if (VUniqueString* FieldName = Entry.Name.Get())
		{
			bool bIsAlreadyInSet;
			Fields.FindOrAdd(FieldName, &bIsAlreadyInSet);
			if (bIsAlreadyInSet)
			{
				continue;
			}
		}
		Entries.Add(Entry);
	}
}

VObject& VClass::NewVObject(FAllocationContext Context, VUniqueStringSet& ArchetypeFields, const TArray<VValue>& ArchetypeValues, TArray<VProcedure*>& OutInitializers)
{
	// Combine the class and archetype to determine which fields will live in the object.
	VEmergentType& NewEmergentType = GetOrCreateEmergentTypeForArchetype(Context, ArchetypeFields, &VObject::StaticCppClassInfo);
	VObject& NewObject = VObject::NewUninitialized(Context, NewEmergentType);

	if (Kind == EKind::Struct)
	{
		NewObject.Misc2 |= IsStructBit;
	}

	// Initialize fields from the archetype.
	// NOTE: This assumes that the order of values matches the IDs of the field set.
	for (auto It = ArchetypeFields.begin(); It != ArchetypeFields.end(); ++It)
	{
		NewObject.SetField(Context, *It->Get(), ArchetypeValues[It.GetId().AsInteger()]);
	}

	// Build the sequence of VProcedures to finish object construction.
	GatherInitializers(ArchetypeFields, OutInitializers);

	return NewObject;
}

UObject* VClass::NewUObject(FAllocationContext Context, VUniqueStringSet& ArchetypeFields, const TArray<VValue>& ArchetypeValues, TArray<VProcedure*>& OutInitializers)
{
	UVerseVMClass* ObjectUClass = CastChecked<UVerseVMClass>(GetOrCreateUClass(Context));

	FStaticConstructObjectParameters Parameters(ObjectUClass);
	// Note: Object will get a default name based on class name
	// TODO: Migrate FSolarisInstantiationScope functionality here to determine Outer and SetFlags
	// TODO: Set instancing graph properly
	Parameters.Outer = GetTransientPackage();
	Parameters.SetFlags = RF_NoFlags;
	UObject* NewObject = StaticConstructObject_Internal(Parameters);

	for (auto It = ArchetypeFields.begin(); It != ArchetypeFields.end(); ++It)
	{
		const VShape::VEntry* Field = ObjectUClass->Shape->GetField(Context, *It->Get());
		checkSlow(Field && Field->Type == EFieldType::FProperty);
		VValue Value = ArchetypeValues[It.GetId().AsInteger()];
		Field->Property->ContainerPtrToValuePtr<VRestValue>(NewObject)->Set(Context, Value);
	}

	// Build the sequence of VProcedures to finish object construction.
	GatherInitializers(ArchetypeFields, OutInitializers);

	return NewObject;
}

void VClass::GatherInitializers(VUniqueStringSet& ArchetypeFields, TArray<VProcedure*>& OutInitializers)
{
	// Build the sequence of VProcedures to finish object construction.
	V_DIE_UNLESS(OutInitializers.IsEmpty());
	OutInitializers.Reserve(Constructor->NumEntries);
	for (uint32 Index = 0; Index < Constructor->NumEntries; ++Index)
	{
		VConstructor::VEntry& Entry = Constructor->Entries[Index];

		// Skip fields which were already initialized by the archetype.
		if (const VUniqueString* Field = Entry.Name.Get())
		{
			FSetElementId ElementId = ArchetypeFields.FindId(Field->AsStringView());
			if (ArchetypeFields.IsValidId(ElementId))
			{
				continue;
			}
		}

		// Record procedures for default initializers and blocks.
		if (VProcedure* Initializer = Entry.Initializer())
		{
			OutInitializers.Add(Initializer);
		}
	}
}

VEmergentType& VClass::GetOrCreateEmergentTypeForArchetype(FAllocationContext Context, VUniqueStringSet& ArchetypeFieldNames, VCppClassInfo* CppClassInfo)
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
		// Only VObjects have space for fields in the object rather than the shape.
		V_DIE_UNLESS(CppClassInfo == &VObject::StaticCppClassInfo);

		// Always store fields from the archetype in the object.
		Fields.Add({Context, Field.Get()}, VShape::VEntry::Offset());
	}
	for (uint32 Index = 0; Index < Constructor->NumEntries; ++Index)
	{
		VConstructor::VEntry& Entry = Constructor->Entries[Index];
		if (VUniqueString* FieldName = Entry.Name.Get())
		{
			if (Entry.bDynamic)
			{
				// Only VObjects have space for fields in the object rather than the shape.
				V_DIE_UNLESS(CppClassInfo == &VObject::StaticCppClassInfo);

				// Store dynamically-initialized and uninitialized fields in the object.
				Fields.FindOrAdd({Context, FieldName}, VShape::VEntry::Offset());
			}
			else
			{
				// Store constant-initialized fields in the shape.
				Fields.FindOrAdd({Context, FieldName}, VShape::VEntry::Constant(Context, Entry.Value.Get()));
			}
		}
	}

	// Compute the shape by interning the set of fields.
	VShape* NewShape = VShape::New(Context, MoveTemp(Fields));
	VEmergentType* NewEmergentType = VEmergentType::New(Context, NewShape, this, CppClassInfo);
	V_DIE_IF(NewEmergentType == nullptr);

	// This new type will then be kept alive in the cache to re-vend if ever the exact same set of fields are used for
	// archetype instantiation of a different object.
	EmergentTypesCache.Add({Context, ArchetypeFieldNames}, {Context, *NewEmergentType});

	return *NewEmergentType;
}

UClass* VClass::CreateUClass(FAllocationContext Context)
{
	ensure(!AssociatedUClass && Kind != EKind::Interface); // Only an actual class should be associated with a UClass

	// 1) Make sure we have a destination package

	FUtf8StringView MangledNameView = UEMangledName && UEMangledName->Num() > 0 ? UEMangledName->AsStringView() : ExtractClassName();
	ensure(MangledNameView.Len() > 0);
	UPackage* ClassPackage = Scope ? Scope->GetOrCreateUPackage(Context, *FString(MangledNameView)) : GetTransientPackage();

	// 2) Create the new UClass object

	FUtf8StringView UEClassNameView = Scope && Scope->GetPackageType() == EPackageType::VNI ? MangledNameView : ExtractClassName();
	const FName UEClassName = UEClassNameView.Len() > 0 ? FName(UEClassNameView) : NAME_None;
	UVerseVMClass* NewClass = NewObject<UVerseVMClass>(ClassPackage, UEClassName, RF_Public | RF_Transient);
	NewClass->Class.Set(Context, this);
#if WITH_EDITOR
	NewClass->SetMetaData(TEXT("IsBlueprintBase"), TEXT("false"));
#endif

	VClass* SuperClass = nullptr;
	if (0 < NumInherited && Inherited[0]->GetKind() == VClass::EKind::Class)
	{
		SuperClass = Inherited[0].Get();
	}
	UClass* SuperUClass = SuperClass ? SuperClass->GetOrCreateUClass(Context) : UObject::StaticClass();
	NewClass->SetSuperStruct(SuperUClass);
	NewClass->ClassConfigName = SuperUClass->ClassConfigName;

	// 3) Generate special shape for it

	VShape::FieldsMap AllFields;
	for (uint32 Index = 0; Index < Constructor->NumEntries; ++Index)
	{
		VConstructor::VEntry& Entry = Constructor->Entries[Index];
		if (VUniqueString* Field = Entry.Name.Get())
		{
			// Store all fields in the UObject, none in the shape
			AllFields.Add({Context, Entry.Name.Get()}, VShape::VEntry::FProperty());
		}
	}
	VShape* ThisShape = VShape::New(Context, MoveTemp(AllFields));
	NewClass->Shape.Set(Context, *ThisShape);

	// 4) Populate its properties

	UVerseVMClass* SuperVerseUClass = Cast<UVerseVMClass>(SuperUClass);
	VShape* SuperShape = SuperVerseUClass ? SuperVerseUClass->Shape.Get() : nullptr;
	FField** PrevProperty = &NewClass->ChildProperties;
	for (auto& Pair : ThisShape->Fields)
	{
		const VShape::VEntry* SuperField = SuperShape ? SuperShape->GetField(Context, *Pair.Key.Get()) : nullptr;
		if (SuperField)
		{
			// If the super shape has it, recycle the same property
			Pair.Value.Property = SuperField->Property;
		}
		else
		{
			// Otherwise create a new property for it
			const FName FieldName = FName(Pair.Key.Get()->AsCString());
			FVRestValueProperty* FieldProperty = new FVRestValueProperty(NewClass, FieldName, RF_NoFlags);
			Pair.Value.Property = FieldProperty;

			*PrevProperty = FieldProperty;
			PrevProperty = &FieldProperty->Next;
		}
	}

	// 5) Finalize class

	NewClass->Bind();
	NewClass->StaticLink(/*bRelinkExistingProperties =*/true);

	AssociatedUClass.Set(Context, NewClass);

	// Don't create the CDO for native classes until they've been bound.
	if (!IsNative())
	{
		AssembleUClass(Context);
	}

	return NewClass;
}

void VClass::AssembleUClass(FAllocationContext Context)
{
	ensure(AssociatedUClass);
	UVerseVMClass* NewClass = Cast<UVerseVMClass>(AssociatedUClass.Get().AsUObject());
	if (NewClass == nullptr)
	{
		return;
	}

	VShape* ThisShape = NewClass->Shape.Get();

	if (NewClass->ClassDefaultObject != nullptr)
	{
		return;
	}

	if (0 < NumInherited && Inherited[0]->GetKind() == VClass::EKind::Class)
	{
		Inherited[0]->AssembleUClass(Context);
	}

	// 6) Create and initialize CDO

	// Collect all UObjects referenced by FProperties and assemble the GC token stream
	NewClass->CollectBytecodeAndPropertyReferencedObjectsRecursively();
	NewClass->AssembleReferenceTokenStream(/*bForce=*/true);

	UObject* CDO = NewClass->GetDefaultObject();
	V_DIE_UNLESS(CDO);
	for (uint32 Index = 0; Index < Constructor->NumEntries; ++Index)
	{
		VConstructor::VEntry& Entry = Constructor->Entries[Index];
		if (const VUniqueString* FieldName = Entry.Name.Get())
		{
			const VShape::VEntry* Field = ThisShape->GetField(Context, *FieldName);
			checkSlow(Field && Field->Type == EFieldType::FProperty);
			if (Entry.bDynamic && !Entry.Value.Get())
			{
				// For the editor: This property has no default and therefore must be specified
				Field->Property->PropertyFlags |= EPropertyFlags::CPF_RequiredParm;
			}
			VRestValue* Slot = Field->Property->ContainerPtrToValuePtr<VRestValue>(CDO);
			new (Slot) VRestValue(0);
			if (!Entry.bDynamic)
			{
				Slot->Set(Context, Entry.Value.Get());
			}
		}
	}
}

bool VClass::SubsumesImpl(FRunningContext Context, VValue Value)
{
	VClass* InputType = nullptr;
	if (VObject* Object = Value.DynamicCast<VObject>())
	{
		VCell* TypeCell = Object->GetEmergentType()->Type.Get();
		checkSlow(TypeCell->IsA<VClass>());
		InputType = static_cast<VClass*>(TypeCell);
	}
	else if (Value.IsUObject())
	{
		InputType = CastChecked<UVerseVMClass>(Value.AsUObject()->GetClass())->Class.Get();
	}
	else
	{
		return false;
	}

	if (InputType == this)
	{
		return true;
	}

	TArray<VClass*, TInlineAllocator<8>> ToCheck;
	auto PushInherited = [&ToCheck](VClass* Class) {
		for (uint32 I = 0; I < Class->NumInherited; ++I)
		{
			ToCheck.Push(Class->Inherited[I].Get());
		}
	};

	PushInherited(InputType);
	while (ToCheck.Num())
	{
		VClass* Class = ToCheck.Pop();
		if (Class == this)
		{
			return true;
		}
		PushInherited(Class);
	}

	return false;
}

FUtf8StringView VClass::ExtractClassName() const
{
	FUtf8StringView ScratchName = GetName();
	if (ScratchName.Len() > 0)
	{
		int StartOfName = ScratchName.Find(":)");
		StartOfName = StartOfName == INDEX_NONE ? 0 : StartOfName + 2;
		int EndOfName = ScratchName.Find("(", StartOfName);
		EndOfName = EndOfName == INDEX_NONE ? ScratchName.Len() : EndOfName;
		ScratchName = ScratchName.SubStr(StartOfName, EndOfName - StartOfName);
	}
	return ScratchName;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
