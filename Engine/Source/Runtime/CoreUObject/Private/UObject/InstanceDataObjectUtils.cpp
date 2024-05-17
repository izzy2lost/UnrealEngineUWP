// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/InstanceDataObjectUtils.h"

#include "HAL/IConsoleManager.h"
#include "Misc/ReverseIterate.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/Field.h"
#include "UObject/Package.h"
#include "UObject/PropertyBagRepository.h"
#include "UObject/PropertyHelper.h"
#include "UObject/PropertyOptional.h"
#include "UObject/PropertyPathNameTree.h"
#include "UObject/UnrealType.h"

static const FName NAME_ValuesSetBySerialization(ANSITEXTVIEW("_ValuesSetBySerialization"));

/** Type used for InstanceDataObject classes. */
class UInstanceDataObjectClass final : public UClass
{
public:
	DECLARE_CASTED_CLASS_INTRINSIC(UInstanceDataObjectClass, UClass, CLASS_Transient, TEXT("/Script/CoreUObject"), CASTCLASS_UClass)

	FByteProperty* ValuesSetBySerializationProperty = nullptr;
};

IMPLEMENT_CORE_INTRINSIC_CLASS(UInstanceDataObjectClass, UClass,
{
});

/** Type used for InstanceDataObject structs to provide support for hashing and custom guids. */
class UInstanceDataObjectStruct final : public UScriptStruct
{
public:
	DECLARE_CASTED_CLASS_INTRINSIC(UInstanceDataObjectStruct, UScriptStruct, CLASS_Transient, TEXT("/Script/CoreUObject"), CASTCLASS_UScriptStruct)

	uint32 GetStructTypeHash(const void* Src) const final;
	FGuid GetCustomGuid() const final { return Guid; }

	FByteProperty* ValuesSetBySerializationProperty = nullptr;
	FGuid Guid;
};

IMPLEMENT_CORE_INTRINSIC_CLASS(UInstanceDataObjectStruct, UScriptStruct,
{
});

uint32 UInstanceDataObjectStruct::GetStructTypeHash(const void* Src) const
{
	class FBoolHash
	{
	public:
		inline void Hash(bool bValue)
		{
			BoolValues = (BoolValues << 1) | (bValue ? 1 : 0);
			if ((++BoolCount & 63) == 0)
			{
				Flush();
			}
		}

		inline uint32 CalculateHash()
		{
			if (BoolCount & 63)
			{
				Flush();
			}
			return BoolHash;
		}

	private:
		inline void Flush()
		{
			BoolHash = HashCombineFast(BoolHash, GetTypeHash(BoolValues));
			BoolValues = 0;
		}

		uint32 BoolHash = 0;
		uint32 BoolCount = 0;
		uint64 BoolValues = 0;
	};

	FBoolHash BoolHash;
	uint32 ValueHash = 0;
	for (TFieldIterator<const FProperty> It(this); It; ++It)
	{
		if (It->GetFName() == NAME_ValuesSetBySerialization)
		{
			continue;
		}
		if (const FBoolProperty* BoolProperty = CastField<const FBoolProperty>(*It))
		{
			for (int32 I = 0; I < It->ArrayDim; ++I)
			{
				BoolHash.Hash(BoolProperty->GetPropertyValue_InContainer(Src, I));
			}
		}
		else if (ensure(It->HasAllPropertyFlags(CPF_HasGetValueTypeHash)))
		{
			for (int32 I = 0; I < It->ArrayDim; ++I)
			{
				uint32 Hash = It->GetValueTypeHash(It->ContainerPtrToValuePtr<void>(Src, I));
				ValueHash = HashCombineFast(ValueHash, Hash);
			}
		}
		else
		{
			ValueHash = HashCombineFast(ValueHash, It->ArrayDim);
		}
	}

	if (const uint32 Hash = BoolHash.CalculateHash())
	{
		ValueHash = HashCombineFast(ValueHash, Hash);
	}

	return ValueHash;
}

namespace UE
{
	static const FName NAME_DisplayName(ANSITEXTVIEW("DisplayName"));
	static const FName NAME_PresentAsTypeMetadata(ANSITEXTVIEW("PresentAsType"));
	static const FName NAME_IsLooseMetadata(ANSITEXTVIEW("IsLoose"));
	static const FName NAME_ContainsLoosePropertiesMetadata(ANSITEXTVIEW("ContainsLooseProperties"));
	static const FName NAME_VerseClass(ANSITEXTVIEW("VerseClass"));
	static const FName NAME_IDOMapKey(ANSITEXTVIEW("Key"));
	static const FName NAME_IDOMapValue(ANSITEXTVIEW("Value"));

	bool bEnableIDOSupport = false;
	FAutoConsoleVariableRef EnableIDOSupportCVar(
		TEXT("IDO.Enable"),
		bEnableIDOSupport,
		TEXT("Allows property bags and IDOs to be created for supported classes.")
	);

	bool IsInstanceDataObjectSupportEnabled(UObject* InObject)
	{
		// Note: NULL is a valid (default) input here; in that case we just return the enable flag.
		bool bIsEnabled = bEnableIDOSupport;
		if (bIsEnabled && InObject && !InObject->IsInPackage(GetTransientPackage()))
		{
			// Property bag placeholder objects are always enabled for IDO support
			if (UE::FPropertyBagRepository::IsPropertyBagPlaceholderObject(InObject))
			{
				return true;
			}

			//@todo FH: change to check trait when available or use config object
			const UClass* ObjClass = InObject->GetClass();
			while (ObjClass && ObjClass->GetClass()->GetFName() != NAME_VerseClass)
			{
				ObjClass = ObjClass->GetSuperClass();
			}

			bIsEnabled = !!ObjClass;
		}

		return bIsEnabled;
	}

	bool StructContainsLooseProperties(const UStruct* Struct)
	{
#if WITH_EDITORONLY_DATA
		return Struct->GetBoolMetaData(NAME_ContainsLoosePropertiesMetadata);
#else
		return false;
#endif
	}

	static UStruct* CreateInstanceDataObjectStructRec(const UClass* StructClass, UStruct* TemplateStruct, UObject* Outer, const FPropertyPathNameTree* PropertyTree);

	template <typename StructType>
	StructType* CreateInstanceDataObjectStructRec(UStruct* TemplateStruct, UObject* Outer, const FPropertyPathNameTree* PropertyTree)
	{
		return CastChecked<StructType>(CreateInstanceDataObjectStructRec(StructType::StaticClass(), TemplateStruct, Outer, PropertyTree));
	}

	static FString UnmanglePropertyName(const FName MaybeMangledName, bool& bOutNameWasMangled)
	{
		FString Result = MaybeMangledName.ToString();
		if (Result.StartsWith(TEXTVIEW("__verse_0x")))
		{
			// chop "__verse_0x" (10 char) + CRC (8 char) + "_" (1 char)
			Result = Result.RightChop(19);
			bOutNameWasMangled = true;
		}
		else
		{
			bOutNameWasMangled = false;
		}
		return Result;
	}

	// recursively re-instances all structs contained by this property to include loose properties
	static void ConvertToInstanceDataObjectProperty(FProperty* Property, FPropertyTypeName PropertyType, UObject* Outer, const FPropertyPathNameTree* PropertyTree)
	{
#if WITH_EDITORONLY_DATA
		if (!Property->HasMetaData(NAME_DisplayName))
		{
			bool bNeedsDisplayName = false;
			FString DisplayName = UnmanglePropertyName(Property->GetFName(), bNeedsDisplayName);
			if (bNeedsDisplayName)
			{
				Property->SetMetaData(NAME_DisplayName, MoveTemp(DisplayName));
			}
		}
#endif

		const auto TrySetContainsLooseProperties = [](FProperty* Property, const FFieldVariant& Inner)
		{
#if WITH_EDITORONLY_DATA
			if (Inner.HasMetaData(NAME_ContainsLoosePropertiesMetadata))
			{
				Property->SetMetaData(NAME_ContainsLoosePropertiesMetadata, TEXT("True"));
			}
#endif
		};

		if (FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
		{
			if (!AsStructProperty->Struct->UseNativeSerialization())
			{
#if WITH_EDITORONLY_DATA
				//@note: Transfer existing metadata over as we build the InstanceDataObject from the struct or it owner, if any, this is useful for testing purposes
				FString OriginalName;
				if (const FString* OriginalType = AsStructProperty->FindMetaData(NAME_OriginalType))
				{
					OriginalName = *OriginalType;
				}
				//@note: To support metadata defined on array of struct in UPROPERTY for testing purposes
				else if (FField* OwnerField = AsStructProperty->Owner.ToField())
				{
					if (const FString* OwnerOriginalType = OwnerField->FindMetaData(NAME_OriginalType))
					{
						OriginalName = *OwnerOriginalType;
					}
				}

				if (OriginalName.IsEmpty())
				{
					UE::FPropertyTypeNameBuilder OriginalNameBuilder;
					OriginalNameBuilder.AddPath(AsStructProperty->Struct);
					OriginalName = WriteToString<256>(OriginalNameBuilder.Build()).ToView();
				}
#endif
				UInstanceDataObjectStruct* Struct = CreateInstanceDataObjectStructRec<UInstanceDataObjectStruct>(AsStructProperty->Struct, Outer, PropertyTree);
				if (const FName StructGuidName = PropertyType.GetParameterName(1); !StructGuidName.IsNone())
				{
					FGuid::Parse(StructGuidName.ToString(), Struct->Guid);
				}
				AsStructProperty->Struct = Struct;
#if WITH_EDITORONLY_DATA
				AsStructProperty->SetMetaData(NAME_OriginalType, *OriginalName);
				AsStructProperty->SetMetaData(NAME_PresentAsTypeMetadata, *OriginalName);
				AsStructProperty->Struct->SetMetaData(NAME_PresentAsTypeMetadata, *OriginalName);

				TrySetContainsLooseProperties(AsStructProperty, AsStructProperty->Struct);
#endif
			}
		}
		else if (FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
		{
			ConvertToInstanceDataObjectProperty(AsArrayProperty->Inner, PropertyType.GetParameter(0), Outer, PropertyTree);
			TrySetContainsLooseProperties(AsArrayProperty, AsArrayProperty->Inner);
		}
		else if (FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
		{
			ConvertToInstanceDataObjectProperty(AsSetProperty->ElementProp, PropertyType.GetParameter(0), Outer, PropertyTree);
			TrySetContainsLooseProperties(AsSetProperty, AsSetProperty->ElementProp);
		}
		else if (FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
		{
			const FPropertyPathNameTree* KeyTree = nullptr;
			const FPropertyPathNameTree* ValueTree = nullptr;
			if (PropertyTree)
			{
				FPropertyPathName Path;
				Path.Push({NAME_IDOMapKey});
				KeyTree = PropertyTree->Find(Path).GetSubTree();
				Path.Pop();
				Path.Push({NAME_IDOMapValue});
				ValueTree = PropertyTree->Find(Path).GetSubTree();
				Path.Pop();
			}

			ConvertToInstanceDataObjectProperty(AsMapProperty->KeyProp, PropertyType.GetParameter(0), Outer, KeyTree);
			TrySetContainsLooseProperties(AsMapProperty, AsMapProperty->KeyProp);
			ConvertToInstanceDataObjectProperty(AsMapProperty->ValueProp, PropertyType.GetParameter(1), Outer, ValueTree);
			TrySetContainsLooseProperties(AsMapProperty, AsMapProperty->ValueProp);
		}
		else if (FOptionalProperty* AsOptionalProperty = CastField<FOptionalProperty>(Property))
		{
			ConvertToInstanceDataObjectProperty(AsOptionalProperty->GetValueProperty(), PropertyType.GetParameter(0), Outer, PropertyTree);
			TrySetContainsLooseProperties(AsOptionalProperty, AsOptionalProperty->GetValueProperty());
		}

#if WITH_EDITORONLY_DATA
		if (Property->GetBoolMetaData(NAME_IsLooseMetadata) || Property->GetBoolMetaData(NAME_ContainsLoosePropertiesMetadata))
		{
			Property->GetOwnerStruct()->SetMetaData(NAME_ContainsLoosePropertiesMetadata, TEXT("True"));
		}
#endif
	}

	// recursively gives a property the metadata and flags of a loose property
	static void MarkPropertyAsLoose(FProperty* Property)
	{
#if WITH_EDITORONLY_DATA
		Property->SetMetaData(NAME_IsLooseMetadata, TEXT("True"));
#endif
		Property->SetPropertyFlags(CPF_Edit | CPF_EditConst);
		if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
		{
			MarkPropertyAsLoose(AsArrayProperty->Inner);
		}
		else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
{
			MarkPropertyAsLoose(AsSetProperty->ElementProp);
		}
		else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
		{
			MarkPropertyAsLoose(AsMapProperty->KeyProp);
			MarkPropertyAsLoose(AsMapProperty->ValueProp);
		}
		else if (const FOptionalProperty* AsOptionalProperty = CastField<FOptionalProperty>(Property))
		{
			MarkPropertyAsLoose(AsOptionalProperty->GetValueProperty());
		}
	}

	// constructs an InstanceDataObject struct by merging the properties in 
	static UStruct* CreateInstanceDataObjectStructRec(const UClass* StructClass, UStruct* TemplateStruct, UObject* Outer, const FPropertyPathNameTree* PropertyTree)
	{
		TSet<FPropertyPathName> SuperPropertyPathsFromTree;

		// UClass is required to inherit from UObject
		UStruct* Super = StructClass->IsChildOf<UClass>() ? UObject::StaticClass() : nullptr;

		if (TemplateStruct)
		{
			{
				const FName SuperName(WriteToString<128>(TemplateStruct->GetName(), TEXTVIEW("_Super")));
				const UClass* SuperStructClass = StructClass->GetSuperClass();
				UStruct* NewSuper = NewObject<UStruct>(Outer, SuperStructClass, MakeUniqueObjectName(nullptr, SuperStructClass, SuperName));
				NewSuper->SetSuperStruct(Super);
				Super = NewSuper;
			}

			// Gather properties for Super Struct
			TArray<FProperty*> SuperProperties;
			for (const FProperty* TemplateProperty : TFieldRange<FProperty>(TemplateStruct))
			{
				FProperty* SuperProperty = CastFieldChecked<FProperty>(FField::Duplicate(TemplateProperty, Super));
				SuperProperties.Add(SuperProperty);

			#if WITH_EDITORONLY_DATA
				FField::CopyMetaData(TemplateProperty, SuperProperty);
			#endif

				FPropertyTypeName Type;
				{
					FPropertyTypeNameBuilder TypeBuilder;
					TemplateProperty->SaveTypeName(TypeBuilder);
					Type = TypeBuilder.Build();
				}

				// Find the sub-tree containing unknown properties for this template property.
				const FPropertyPathNameTree* SubTree = nullptr;
				if (PropertyTree)
				{
					FPropertyPathName Path;
					Path.Push({TemplateProperty->GetFName(), Type});
					if (FPropertyPathNameTree::FConstNode Node = PropertyTree->Find(Path))
					{
						SubTree = Node.GetSubTree();
						SuperPropertyPathsFromTree.Add(MoveTemp(Path));
					}
				}

				ConvertToInstanceDataObjectProperty(SuperProperty, Type, Super, SubTree);
			}

			// AddCppProperty expects reverse property order for StaticLink to work correctly
			for (FProperty* Property : ReverseIterate(SuperProperties))
			{
				Super->AddCppProperty(Property);
			}
			Super->Bind();
			Super->StaticLink(/*bRelinkExistingProperties*/true);
		}

		const FName InstanceDataObjectName = (TemplateStruct) ? FName(WriteToString<128>(TemplateStruct->GetName(), TEXTVIEW("_InstanceDataObject"))) : FName(TEXTVIEW("InstanceDataObject"));
		UStruct* Result = NewObject<UStruct>(Outer, StructClass, MakeUniqueObjectName(nullptr, StructClass, InstanceDataObjectName));
		Result->SetSuperStruct(Super);

		// Gather "loose" properties for child Struct
		TArray<FProperty*> LooseInstanceDataObjectProperties;
		if (PropertyTree)
		{
			for (FPropertyPathNameTree::FConstIterator It = PropertyTree->CreateConstIterator(); It; ++It)
			{
				FName Name = It.GetName();
				if (Name == NAME_ValuesSetBySerialization)
				{
					// in rare cases, the ValuesSetBySerialization property will get serialized/deserialized even though it's transient.
					// Since this property is regenerated by this system and should never be loose, ignore it fully here.
					continue;
				}
				FPropertyTypeName Type = It.GetType();
				FPropertyPathName Path;
				Path.Push({Name, Type});
				if (!SuperPropertyPathsFromTree.Contains(Path))
				{
					// Construct a property from the type and try to use it to serialize the value.
					FField* Field = FField::TryConstruct(Type.GetName(), Result, Name, RF_NoFlags);
					if (FProperty* Property = CastField<FProperty>(Field); Property && Property->LoadTypeName(Type, It.GetNode().GetTag()))
					{
						MarkPropertyAsLoose(Property);
						ConvertToInstanceDataObjectProperty(Property, Type, Result, It.GetNode().GetSubTree());
						LooseInstanceDataObjectProperties.Add(Property);
						continue;
					}
					delete Field;
				}
			}
		}

		// Add a hidden byte array property to record whether its sibling properties were set by serialization.
		FByteProperty* ValuesSetBySerializationProperty = CastFieldChecked<FByteProperty>(FByteProperty::Construct(Result, NAME_ValuesSetBySerialization, RF_Transient | RF_MarkAsNative));
		{
			ValuesSetBySerializationProperty->SetPropertyFlags(CPF_Transient | CPF_EditorOnly | CPF_NativeAccessSpecifierPrivate);
			Result->AddCppProperty(ValuesSetBySerializationProperty);
		}

		// Store generated properties to avoid scanning every property to find it when it is needed.
		if (UInstanceDataObjectClass* IdoClass = Cast<UInstanceDataObjectClass>(Result))
		{
			IdoClass->ValuesSetBySerializationProperty = ValuesSetBySerializationProperty;
		}
		else if (UInstanceDataObjectStruct* IdoStruct = Cast<UInstanceDataObjectStruct>(Result))
		{
			IdoStruct->ValuesSetBySerializationProperty = ValuesSetBySerializationProperty;
		}

		// AddCppProperty expects reverse property order for StaticLink to work correctly
		for (FProperty* Property : ReverseIterate(LooseInstanceDataObjectProperties))
		{
			Result->AddCppProperty(Property);
		}

		// Count properties and set the size of the array of flags.
		int32 PropertyCount = -1; // Start at -1 to exclude ValuesSetBySerialization.
		for (TFieldIterator<FProperty> It(Result); It; ++It)
		{
			PropertyCount += It->ArrayDim;
		}
		ValuesSetBySerializationProperty->ArrayDim = FMath::DivideAndRoundUp(PropertyCount, 8);

		Result->Bind();
		Result->StaticLink(/*bRelinkExistingProperties*/true);
		return Result;
	}

	void CopyCDO(const UObject* Source, UObject* Destination)
	{
		for (const FProperty* SourceProperty : TFieldRange<FProperty>(Source->GetClass()))
		{
			if (const FProperty* DestinationProperty = Destination->GetClass()->FindPropertyByName(SourceProperty->GetFName()))
			{
				if (SourceProperty->SameType(DestinationProperty))
				{
					const void* SourceValue = SourceProperty->ContainerPtrToValuePtr<void>(Source);
					void* DestinationValue = DestinationProperty->ContainerPtrToValuePtr<void>(Destination);
					DestinationProperty->CopyCompleteValue(DestinationValue, SourceValue);
				}
				else
				{
					FString ValueText;
					const void* SourceValue = SourceProperty->ContainerPtrToValuePtr<void>(Source);
					void* DestinationValue = DestinationProperty->ContainerPtrToValuePtr<void>(Destination);
					SourceProperty->ExportText_Direct(ValueText, SourceValue, SourceValue, const_cast<UObject*>(Source), PPF_None);
					DestinationProperty->ImportText_Direct(*ValueText, DestinationValue, Destination, PPF_None);
				}
			}
		}
	}

	static void SetClassFlags(UClass* IDOClass, const UClass* OwnerClass)
	{
		// always set
		IDOClass->AssembleReferenceTokenStream();
		IDOClass->ClassFlags |= CLASS_NotPlaceable | CLASS_Hidden | CLASS_HideDropDown;
		
		// copy flags from OwnerClass
		IDOClass->ClassFlags |= OwnerClass->ClassFlags & (
			CLASS_EditInlineNew | CLASS_CollapseCategories | CLASS_Const | CLASS_CompiledFromBlueprint | CLASS_HasInstancedReference);
	}

	UClass* CreateInstanceDataObjectClass(const FPropertyPathNameTree* PropertyTree, UClass* OwnerClass, UObject* Outer)
	{
		UClass* Result = CreateInstanceDataObjectStructRec<UInstanceDataObjectClass>(OwnerClass, Outer, PropertyTree);
#if WITH_EDITORONLY_DATA
		if (const FString& DisplayName = OwnerClass->GetMetaData(NAME_DisplayName); !DisplayName.IsEmpty())
		{
			Result->SetMetaData(NAME_DisplayName, *DisplayName);
		}
#endif

		SetClassFlags(Result, OwnerClass);

		const UObject* OwnerCDO = OwnerClass->GetDefaultObject();
		UObject* ResultCDO = Result->GetDefaultObject();
		if (ensure(OwnerCDO && ResultCDO))
		{
			CopyCDO(OwnerCDO, ResultCDO);
		}
		return Result;
	}

	static const FByteProperty* FindValuesSetBySerializationProperty(const UStruct* Struct)
	{
		if (const UInstanceDataObjectClass* IdoClass = Cast<UInstanceDataObjectClass>(Struct))
		{
			return IdoClass->ValuesSetBySerializationProperty;
		}
		if (const UInstanceDataObjectStruct* IdoStruct = Cast<UInstanceDataObjectStruct>(Struct))
		{
			return IdoStruct->ValuesSetBySerializationProperty;
		}
		return CastField<FByteProperty>(Struct->FindPropertyByName(NAME_ValuesSetBySerialization));
	}

	void MarkPropertySetBySerialization(const UStruct* Struct, void* StructData, const FProperty* Property, int32 ArrayIndex)
	{
	#if WITH_EDITORONLY_DATA
		if (const FByteProperty* ValuesSetBySerializationProperty = FindValuesSetBySerializationProperty(Struct))
		{
			const int32 PropertyIndex = Property->GetIndexInOwner() + ArrayIndex;
			const int32 ByteIndex = PropertyIndex / 8;
			const int32 BitOffset = PropertyIndex % 8;
			if (ByteIndex < ValuesSetBySerializationProperty->ArrayDim)
			{
				uint8* PropertyDataPtr = ValuesSetBySerializationProperty->ContainerPtrToValuePtr<uint8>(StructData, ByteIndex);
				*PropertyDataPtr |= (1 << BitOffset);
			}
		}
	#endif
	}

	bool WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex)
	{
	#if WITH_EDITORONLY_DATA
		if (const FByteProperty* ValuesSetBySerializationProperty = FindValuesSetBySerializationProperty(Struct))
		{
			const int32 PropertyIndex = Property->GetIndexInOwner() + ArrayIndex;
			const int32 ByteIndex = PropertyIndex / 8;
			const int32 BitOffset = PropertyIndex % 8;
			if (ByteIndex < ValuesSetBySerializationProperty->ArrayDim)
			{
				const uint8* PropertyDataPtr = ValuesSetBySerializationProperty->ContainerPtrToValuePtr<uint8>(StructData, ByteIndex);
				return (*PropertyDataPtr & (1 << BitOffset)) != 0;
			}
		}
	#endif
		return false;
	}

	void CopyPropertySetBySerializationData(const FFieldVariant& OldField, void* OldDataPtr, const FFieldVariant& NewField, void* NewDataPtr)
	{
		if (const FStructProperty* OldAsStructProperty = OldField.Get<FStructProperty>())
		{
			const FStructProperty* NewAsStructProperty = NewField.Get<FStructProperty>();
			checkf(NewAsStructProperty, TEXT("Type mismatch between OldField and NewField. Expected FStructProperty"));
			CopyPropertySetBySerializationData(OldAsStructProperty->Struct, OldDataPtr, NewAsStructProperty->Struct, NewDataPtr);
		}
		else if (const FArrayProperty* OldAsArrayProperty = OldField.Get<FArrayProperty>())
		{
			const FArrayProperty* NewAsArrayProperty = NewField.Get<FArrayProperty>();
			checkf(NewAsArrayProperty, TEXT("Type mismatch between OldField and NewField. Expected FArrayProperty"));
			
			FScriptArrayHelper OldArrayHelper(OldAsArrayProperty, OldDataPtr);
			FScriptArrayHelper NewArrayHelper(NewAsArrayProperty, NewDataPtr);
			for (int32 ArrayIndex = 0; ArrayIndex < OldArrayHelper.Num(); ++ArrayIndex)
			{
				if (NewArrayHelper.IsValidIndex(ArrayIndex))
				{
					CopyPropertySetBySerializationData(
						OldAsArrayProperty->Inner, OldArrayHelper.GetElementPtr(ArrayIndex),
						NewAsArrayProperty->Inner, NewArrayHelper.GetElementPtr(ArrayIndex));
				}
			}
		}
		else if (const FSetProperty* OldAsSetProperty = OldField.Get<FSetProperty>())
		{
			const FSetProperty* NewAsSetProperty = NewField.Get<FSetProperty>();
			checkf(NewAsSetProperty, TEXT("Type mismatch between OldField and NewField. Expected FSetProperty"));
			
			FScriptSetHelper OldSetHelper(OldAsSetProperty, OldDataPtr);
			FScriptSetHelper NewSetHelper(NewAsSetProperty, NewDataPtr);
			FScriptSetHelper::FIterator OldItr = OldSetHelper.CreateIterator();
			FScriptSetHelper::FIterator NewItr = NewSetHelper.CreateIterator();
			
			for (; OldItr && NewItr; ++OldItr, ++NewItr)
			{
				CopyPropertySetBySerializationData(
					OldAsSetProperty->ElementProp, OldSetHelper.GetElementPtr(OldItr),
					NewAsSetProperty->ElementProp, NewSetHelper.GetElementPtr(NewItr));
			}
		}
		else if (const FMapProperty* OldAsMapProperty = OldField.Get<FMapProperty>())
		{
			const FMapProperty* NewAsMapProperty = NewField.Get<FMapProperty>();
			checkf(NewAsMapProperty, TEXT("Type mismatch between OldField and NewField. Expected FMapProperty"));
			
			FScriptMapHelper OldMapHelper(OldAsMapProperty, OldDataPtr);
			FScriptMapHelper NewMapHelper(NewAsMapProperty, NewDataPtr);
			FScriptMapHelper::FIterator OldItr = OldMapHelper.CreateIterator();
			FScriptMapHelper::FIterator NewItr = NewMapHelper.CreateIterator();
			
			for (; OldItr && NewItr; ++OldItr, ++NewItr)
			{
				CopyPropertySetBySerializationData(
					OldAsMapProperty->KeyProp, OldMapHelper.GetKeyPtr(OldItr),
					NewAsMapProperty->KeyProp, NewMapHelper.GetKeyPtr(NewItr));
				CopyPropertySetBySerializationData(
					OldAsMapProperty->ValueProp, OldMapHelper.GetValuePtr(OldItr),
					NewAsMapProperty->ValueProp, NewMapHelper.GetValuePtr(NewItr));
			}
		}
		else if (UStruct* OldAsStruct = OldField.Get<UStruct>())
		{
			const UStruct* NewAsStruct = NewField.Get<UStruct>();
			checkf(NewAsStruct, TEXT("Type mismatch between OldField and NewField. Expected UStruct"));

			auto FindMatchingProperty = [](const UStruct* Struct, const FProperty* Property) -> const FProperty*
			{
				for (const FProperty* StructProperty : TFieldRange<FProperty>(Struct))
				{
					if (StructProperty->GetFName() == Property->GetFName() && StructProperty->GetID() == Property->GetID())
					{
						return StructProperty;
					}
				}
				return nullptr;
			};
			for (const FProperty* OldSubProperty : TFieldRange<FProperty>(OldAsStruct))
			{
				if (const FProperty* NewSubProperty = FindMatchingProperty(NewAsStruct, OldSubProperty))
				{
					for (int32 ArrayIndex = 0; ArrayIndex < FMath::Min(OldSubProperty->ArrayDim, NewSubProperty->ArrayDim); ++ArrayIndex)
					{
						// copy set flags to new struct instance
						if (WasPropertySetBySerialization(OldAsStruct, OldDataPtr, NewSubProperty, ArrayIndex))
						{
							MarkPropertySetBySerialization(NewAsStruct, NewDataPtr, NewSubProperty, ArrayIndex);
						}
					
						// recurse
						CopyPropertySetBySerializationData(
							OldSubProperty, OldSubProperty->ContainerPtrToValuePtr<void>(OldDataPtr, ArrayIndex),
							NewSubProperty, NewSubProperty->ContainerPtrToValuePtr<void>(NewDataPtr, ArrayIndex));
					}
				}
			}
		}
	}
} // UE
