// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/InstanceDataObjectUtils.h"
#include "UObject/PropertyBag.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/Field.h"
#include "HAL/IConsoleManager.h"

namespace UE
{
	// typedef to help make it clearer when a pathName has indices and when the indices are wildcarded away
	using FWildcardPropertyPathName = FPropertyPathName;
	
	static FName Name_ValuesSetBySerialization(TEXT("_ValuesSetBySerialization"));
	static const FName NAME_StructOriginalTypeMetadata(TEXT("OriginalType"));
	static const FName NAME_PresentAsTypeMetadata(TEXT("PresentAsType"));
	static const FName NAME_IsLooseMetadata(TEXT("IsLoose"));
	static const FName NAME_VerseClass("VerseClass");
	
	struct ResolvePropertyPathNameHelperParams
	{
		void* Data = nullptr;
		FProperty* ResultProperty = nullptr;
		const UE::FPropertyPathName& Path;
		int32 CurPathIndex = 0;
		int32 EndPathIndex = INDEX_NONE; // INDEX_NONE has the same behavior as Path.GetSegmentCount()
		bool bAddIfNeeded = false;
	};

	bool bEnableIDOSupport = false;
	FAutoConsoleVariableRef EnableIDOSupportCVar(
		TEXT("IDO.Enable"),
		bEnableIDOSupport,
		TEXT("Allows property bags and IDOs to be created for supported classes.")
	);

	bool IsInstanceDataObjectSupportEnabled(UObject* InObject)
	{
		return bEnableIDOSupport
			//@todo FH: change to check trait when available or use config object
			&& (!InObject || InObject->GetClass()->GetClass()->GetFName() == NAME_VerseClass);
	}

	static void BuildSegmentTypeFromProperty(const FProperty* Property, FPropertyTypeNameBuilder& OutType)
	{
#if false // TODO: @jordan.hoffmann when complete type info is finished use this branch instead
		OutType.AddName(Property->GetID());
		if (const FObjectProperty* AsObjectProperty = CastField<FObjectProperty>(Property))
		{
			OutType.BeginParameters();
			OutType.AddName(AsObjectProperty->GetID());
			OutType.EndParameters();
		}
		else if (const FEnumProperty* AsEnumProperty = CastField<FEnumProperty>(Property))
		{
			OutType.BeginParameters();
			OutType.AddPath(AsEnumProperty->GetEnum());
			OutType.EndParameters();
		}
		else if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
		{
			OutType.BeginParameters();
			BuildSegmentTypeFromProperty(AsArrayProperty->Inner, OutType);
			OutType.EndParameters();
		}
		else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
		{
			OutType.BeginParameters();
			BuildSegmentTypeFromProperty(AsSetProperty->ElementProp, OutType);
			OutType.EndParameters();
		}
		else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
		{
			OutType.BeginParameters();
			BuildSegmentTypeFromProperty(AsMapProperty->KeyProp, OutType);
			BuildSegmentTypeFromProperty(AsMapProperty->ValueProp, OutType);
			OutType.EndParameters();
		}
#else
		OutType.AddName(Property->GetID());
		if (const FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
		{
			OutType.BeginParameters();
			OutType.AddName(AsStructProperty->Struct->GetFName());
			OutType.EndParameters();
		}
		else if (const FEnumProperty* AsEnumProperty = CastField<FEnumProperty>(Property))
		{
			OutType.BeginParameters();
			OutType.AddName(AsEnumProperty->GetEnum()->GetFName());
			OutType.EndParameters();
		}
		else if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
		{
			OutType.BeginParameters();
			if (AsArrayProperty->Inner->IsA<FEnumProperty>())
			{
				// enum paths currently don't recurse when they're in containers
				OutType.AddName(AsArrayProperty->Inner->GetID());
			}
			else
			{
				BuildSegmentTypeFromProperty(AsArrayProperty->Inner, OutType);
			}
			OutType.EndParameters();
		}
		else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
		{
			// sets currently don't recurse their element property types
			OutType.BeginParameters();
			OutType.AddName(AsSetProperty->ElementProp->GetID());
			OutType.EndParameters();
		}
		else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
		{
			// maps currently don't recurse their key/value property types
			OutType.BeginParameters();
			OutType.AddName(AsMapProperty->KeyProp->GetID());
			OutType.AddName(AsMapProperty->ValueProp->GetID());
			OutType.EndParameters();
		}
#endif
	}
	
	static FPropertyTypeName GetSegmentTypeFromProperty(const FProperty* Property)
	{
		FPropertyTypeNameBuilder TypeBuilder;
		BuildSegmentTypeFromProperty(Property, TypeBuilder);
		return TypeBuilder.Build();
	}
	
	static bool ResolvePropertyPathNameHelper(const UStruct* Struct, ResolvePropertyPathNameHelperParams& Params);
	static bool ResolvePropertyPathNameHelper(ResolvePropertyPathNameHelperParams& Params)
	{
		if (Params.CurPathIndex == Params.Path.GetSegmentCount() || Params.CurPathIndex == Params.EndPathIndex)
		{
			return true;
		}
		FPropertyPathNameSegment Segment = Params.Path.GetSegment(Params.CurPathIndex);
		if (const FStructProperty* AsStructProperty = CastField<FStructProperty>(Params.ResultProperty))
		{
			return ResolvePropertyPathNameHelper(AsStructProperty->Struct, Params);
		}
		if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Params.ResultProperty))
		{
			FScriptArrayHelper Array(AsArrayProperty, Params.Data);
			if (!Array.IsValidIndex(Segment.Index))
			{
				if (Params.bAddIfNeeded)
				{
					Array.Resize(Segment.Index + 1);
				}
				else
				{
					return false;
				}
			}
			++Params.CurPathIndex;
			Params.ResultProperty = AsArrayProperty->Inner;
			Params.Data = Array.GetElementPtr(Segment.Index);
			return ResolvePropertyPathNameHelper(Params);
		}
		if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Params.ResultProperty))
		{
			FScriptSetHelper Set(AsSetProperty, Params.Data);
			if (!Set.IsValidIndex(Segment.Index))
			{
				if (Params.bAddIfNeeded)
				{
					Segment.Index = Set.AddDefaultValue_Invalid_NeedsRehash();
				}
				else
				{
					return false;
				}
			}
			++Params.CurPathIndex;
			Params.ResultProperty = AsSetProperty->ElementProp;
			Params.Data = Set.GetElementPtr(Segment.Index);
			return ResolvePropertyPathNameHelper(Params);
		}
		if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Params.ResultProperty))
		{
			FScriptMapHelper Map(AsMapProperty, Params.Data);
			if (!Map.IsValidIndex(Segment.Index))
			{
				if (Params.bAddIfNeeded)
				{
					Segment.Index = Map.AddDefaultValue_Invalid_NeedsRehash();
				}
				else
				{
					return false;
				}
			}
			++Params.CurPathIndex;
			Params.ResultProperty = AsMapProperty->KeyProp;
			Params.Data = Map.GetKeyPtr(Segment.Index);
			if (ResolvePropertyPathNameHelper(Params))
			{
				return true;
			}
			Params.ResultProperty = AsMapProperty->ValueProp;
			Params.Data = Map.GetValuePtr(Segment.Index);
			return ResolvePropertyPathNameHelper(Params);
		}
		check(Params.CurPathIndex == Params.Path.GetSegmentCount() - 1)
		return true;
	}
	
	static bool ResolvePropertyPathNameHelper(const UStruct* Struct, ResolvePropertyPathNameHelperParams& Params)
    {
		const FPropertyPathNameSegment Segment = Params.Path.GetSegment(Params.CurPathIndex);
		for (FProperty* Property : TFieldRange<FProperty>(Struct))
		{
			// find a property that matches the segment
			const FPropertyTypeName PropertySegmentType = GetSegmentTypeFromProperty(Property);
			if (Segment.Name != Property->GetFName() || Segment.Type != PropertySegmentType)
			{
				continue;
			}
			const bool bStaticArrayIndex = !Property->IsA<FArrayProperty>() && !Property->IsA<FSetProperty>() && !Property->IsA<FMapProperty>();
			
			if (bStaticArrayIndex && Segment.Index != INDEX_NONE)
			{
				if (Segment.Index < Property->ArrayDim)
				{
					++Params.CurPathIndex;
					Params.ResultProperty = Property;
					Params.Data = Property->ContainerPtrToValuePtr<void>(Params.Data, Segment.Index);
					return ResolvePropertyPathNameHelper(Params);
				}
				else
				{
					// out of static array bounds
					return false;
				}
			}
			else
			{
				if (Segment.Index == INDEX_NONE)
				{
					++Params.CurPathIndex;
				}
				Params.ResultProperty = Property;
				Params.Data = Property->ContainerPtrToValuePtr<void>(Params.Data);
				return ResolvePropertyPathNameHelper(Params);
			}
		}
		
		return false; // segment not found in struct
    }

	static UStruct* CreateInstanceDataObjectStructRec(const UClass* StructClass, UStruct* TemplateStruct,
		UObject* Outer, const TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>>& LooseProperties, FWildcardPropertyPathName& Path);
	template <typename TStructType>
	TStructType* CreateInstanceDataObjectStructRec(UStruct* TemplateStruct, UObject* Outer,
		const TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>>& LooseProperties, FWildcardPropertyPathName& Path)
	{
		return CastChecked<TStructType>(CreateInstanceDataObjectStructRec(TStructType::StaticClass(), TemplateStruct, Outer, LooseProperties, Path));
	}

	static FPropertyPathNameSegment CreateSegmentFromProperty(const FProperty* Inner, int32 Index = INDEX_NONE)
	{
		FPropertyPathNameSegment Result;
		Result.Index = INDEX_NONE; 
		Result.Name = Inner->GetFName();
		Result.Type = GetSegmentTypeFromProperty(Inner);
		return Result;
	}

	// recursively re-instances all structs contained by this property to include loose properties
	static void ConvertToInstanceDataObjectProperty(FProperty* Property, UObject* Outer,
		const TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>>& LooseProperties, FWildcardPropertyPathName& Path)
	{
		if (FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
		{
			if (!AsStructProperty->Struct->UseNativeSerialization())
			{
#if WITH_EDITORONLY_DATA
				//@note: Transfer existing metadata over as we build the InstanceDataObject from the struct or it owner, if any, this is useful for testing purposes
				FString OriginalName;
				if (const FString* OriginalType = AsStructProperty->FindMetaData(NAME_StructOriginalTypeMetadata))
				{
					OriginalName = *OriginalType;
				}
				//@note: To support metadata defined on array of struct in UPROPERTY for testing purposes
				else if (FField* OwnerField = AsStructProperty->Owner.ToField())
				{
					if (const FString* OwnerOriginalType = OwnerField->FindMetaData(NAME_StructOriginalTypeMetadata))
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
				AsStructProperty->Struct = CreateInstanceDataObjectStructRec<UScriptStruct>(AsStructProperty->Struct, Outer, LooseProperties, Path);
#if WITH_EDITORONLY_DATA
				AsStructProperty->SetMetaData(NAME_StructOriginalTypeMetadata, *OriginalName);
				AsStructProperty->SetMetaData(NAME_PresentAsTypeMetadata, *OriginalName);
				AsStructProperty->Struct->SetMetaData(NAME_PresentAsTypeMetadata, *OriginalName);
#endif
			}
		}
		else if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
		{
			ConvertToInstanceDataObjectProperty(AsArrayProperty->Inner, Outer, LooseProperties, Path);
		}
		else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
		{
			ConvertToInstanceDataObjectProperty(AsSetProperty->ElementProp, Outer, LooseProperties, Path);
		}
		else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
		{
			// todo: This will likely need revisiting once devin has maps working
			Path.Push(CreateSegmentFromProperty(AsMapProperty->KeyProp));
			ConvertToInstanceDataObjectProperty(AsMapProperty->KeyProp, Outer, LooseProperties, Path);
			Path.Pop();
			
			Path.Push(CreateSegmentFromProperty(AsMapProperty->ValueProp));
			ConvertToInstanceDataObjectProperty(AsMapProperty->ValueProp, Outer, LooseProperties, Path);
			Path.Pop();
		}
	}
	
	// copy template property then convert it into an InstanceDataObject property by adding loose properties
	static FProperty* CreateInstanceDataObjectProperty(const FProperty* TemplateProperty, UObject* Outer,
		const TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>>& LooseProperties, FWildcardPropertyPathName& Path)
	{
		FProperty* InstanceDataObjectProperty = CastFieldChecked<FProperty>(FField::Duplicate(TemplateProperty, Outer));
#if WITH_EDITORONLY_DATA
		FField::CopyMetaData(TemplateProperty, InstanceDataObjectProperty);
#endif
		ConvertToInstanceDataObjectProperty(InstanceDataObjectProperty, Outer, LooseProperties, Path);
		return InstanceDataObjectProperty;
	}

	// return a copy of Path with all the indices set to -1. This way all container elements will have the same wildcard path
	static FWildcardPropertyPathName ConvertToWildcardPath(const FPropertyPathName& Path)
	{
		FWildcardPropertyPathName Result = Path;
		// make path a wildcard path
		for (int I = 0; I < Result.GetSegmentCount(); ++I)
		{
			FPropertyPathNameSegment Segment = Result.GetSegment(I);
			Segment.Index = INDEX_NONE;
			Result.SetSegment(I, Segment);
		}
		return Result;
	}

	// recursively add all the wildcard paths of both Property and all it's sub-Properties to OutLooseProperties
	static void AddWildcardedProperties(TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>>& OutProperties, FWildcardPropertyPathName& ParentPath, const FProperty* Property)
	{
		OutProperties.FindOrAdd(ParentPath).Add(Property->GetFName(), Property);
		
		ParentPath.Push(CreateSegmentFromProperty(Property));
		if (const FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
		{
			for (const FProperty* SubProperty : TFieldRange<FProperty>(AsStructProperty->Struct))
			{
				AddWildcardedProperties(OutProperties, ParentPath, SubProperty);
			}
		}
		else if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
		{
			AddWildcardedProperties(OutProperties, ParentPath, AsArrayProperty->Inner);
		}
		else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
		{
			AddWildcardedProperties(OutProperties, ParentPath, AsSetProperty->ElementProp);
		}
		else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
		{
			AddWildcardedProperties(OutProperties, ParentPath, AsMapProperty->KeyProp);
			AddWildcardedProperties(OutProperties, ParentPath, AsMapProperty->ValueProp);
		}
		ParentPath.Pop();
	}

	// construct a map that keys a parent struct by it's wildcard path and returns an array of all it's loose properties
	static TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>> GetWildcardedLooseProperties(const FPropertyBag* PropertyBag)
	{
		
		TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>> LooseProperties;
		if (PropertyBag)
		{
			for (FPropertyBag::FConstIterator Itr = PropertyBag->CreateConstIterator(); Itr; ++Itr)
			{
				FWildcardPropertyPathName ParentPath = ConvertToWildcardPath(Itr.GetPath());
				ParentPath.Pop();
				const FProperty* Property = Itr.GetProperty();
				if (ensure(Property))
				{
					AddWildcardedProperties(LooseProperties, ParentPath, Property);
				}
			}
		}
		
		return LooseProperties;
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
        else if (const FSetProperty* AsSetProperty =CastField<FSetProperty>(Property))
        {
			MarkPropertyAsLoose(AsSetProperty->ElementProp);
        }
        else if (const FMapProperty* AsMapProperty =CastField<FMapProperty>(Property))
        {
			MarkPropertyAsLoose(AsMapProperty->KeyProp);
			MarkPropertyAsLoose(AsMapProperty->ValueProp);
        }
	}

	// constructs an InstanceDataObject struct by merging the properties in 
	static UStruct* CreateInstanceDataObjectStructRec(const UClass* StructClass, UStruct* TemplateStruct,
		UObject* Outer, const TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>>& LooseProperties, FWildcardPropertyPathName& Path)
	{
		UStruct* Super = nullptr;

		const TMap<FName, const FProperty*>* BagProperties = LooseProperties.Find(Path);

		auto MatchesBagProperty = [&BagProperties](const FProperty* Property)
		{
			if (BagProperties)
			{
				if (const FProperty* const* Found = BagProperties->Find(Property->GetFName()))
				{
					return (*Found)->SameType(Property);
				}
			}
			return false;
		};
		
		if (TemplateStruct)
		{
			const FName SuperName(TemplateStruct->GetName() + TEXT("_Super"));
			Super = NewObject<UStruct>(Outer, StructClass, MakeUniqueObjectName(nullptr, StructClass, SuperName));
			
			// Gather properties for Super Struct
			TArray<FProperty*> SuperProperties;
			for (const FProperty* TemplateProperty : TFieldRange<FProperty>(TemplateStruct))
			{
				if (MatchesBagProperty(TemplateProperty))
				{
					// this property was determined to be loose despite it being in the template.
					// this likely occured due to an entire struct instance being loose and that instance becoming a template
					continue;
				}
				Path.Push(CreateSegmentFromProperty(TemplateProperty));
				FProperty* SuperProperty = CreateInstanceDataObjectProperty(TemplateProperty, Super, LooseProperties, Path);
				
				Path.Pop();
				SuperProperties.Add(SuperProperty);
			}

			if (StructClass == UClass::StaticClass())
			{
				// UClasses are required to inherit from a UObject class
				Super->SetSuperStruct(UObject::StaticClass());
			}
		    
			// AddCppProperty expects reverse property order for StaticLink to work correctly
			for (int32 I = SuperProperties.Num() - 1; I >= 0; --I)
			{
				Super->AddCppProperty(SuperProperties[I]);
			}
			Super->Bind();
			Super->StaticLink(/*RelinkExistingProperties*/true);
		}
		else if (StructClass == UClass::StaticClass())
		{
			// UClasses are required to inherit from a UObject class
			Super = UObject::StaticClass();
		}

		const FName InstanceDataObjectName = (TemplateStruct) ? FName(TemplateStruct->GetName() + TEXT("_InstanceDataObject")) : FName(TEXT("InstanceDataObject"));
		UStruct* Result = NewObject<UStruct>(Outer, StructClass, MakeUniqueObjectName(nullptr, StructClass, InstanceDataObjectName));

		// Gather "loose" properties for child Struct
		TArray<FProperty*> LooseInstanceDataObjectProperties;
		if (BagProperties)
		{
			for (const TPair<FName, const FProperty*>& BagProperty : *BagProperties)
			{
				Path.Push(CreateSegmentFromProperty(BagProperty.Value));
				FProperty* LooseProperty = CreateInstanceDataObjectProperty(BagProperty.Value, Result, LooseProperties, Path);
				Path.Pop();
				
				MarkPropertyAsLoose(LooseProperty);
				LooseInstanceDataObjectProperties.Add(LooseProperty);
			}
		}

		// add a hidden set property used to record whether this struct's properties were set serialization.
		{
			FSetProperty* ValuesSetBySerializationProperty = CastFieldChecked<FSetProperty>(FSetProperty::Construct(Result, Name_ValuesSetBySerialization, RF_Transient | RF_MarkAsNative));
			static FName Name_PropertyName(TEXT("PropertyName"));
			ValuesSetBySerializationProperty->ElementProp = CastFieldChecked<FProperty>(FInt64Property::Construct(ValuesSetBySerializationProperty, Name_PropertyName, RF_Transient));
			ValuesSetBySerializationProperty->SetPropertyFlags(CPF_Transient | CPF_EditorOnly | CPF_NativeAccessSpecifierPrivate);
			Result->AddCppProperty(ValuesSetBySerializationProperty);
		}

		Result->SetSuperStruct(Super);
		
		// AddCppProperty expects reverse property order for StaticLink to work correctly
		for (int32 I = LooseInstanceDataObjectProperties.Num() - 1; I >= 0; --I)
		{
			Result->AddCppProperty(LooseInstanceDataObjectProperties[I]);
		}
		Result->Bind();
		Result->StaticLink(/*RelinkExistingProperties*/true);
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
	
	UClass* CreateInstanceDataObjectClass(const FPropertyBag* PropertyBag, UClass* OwnerClass, UObject* Outer)
	{
		const TMap<FWildcardPropertyPathName, TMap<FName, const FProperty*>> LooseProperties = GetWildcardedLooseProperties(PropertyBag);
		FWildcardPropertyPathName ParentPath;
		UClass* Result = CreateInstanceDataObjectStructRec<UClass>(OwnerClass, Outer, LooseProperties, ParentPath);
#if WITH_EDITORONLY_DATA
		const FString& DisplayName = OwnerClass->GetMetaData(TEXT("DisplayName"));
		if (!DisplayName.IsEmpty())
		{
			Result->SetMetaData(TEXT("DisplayName"), *DisplayName);
		}
#endif

		const UObject* OwnerCDO = OwnerClass->GetDefaultObject(true);
		UObject* ResultCDO = Result->GetDefaultObject(true);
		if (ensure(OwnerCDO && ResultCDO))
		{
			CopyCDO(OwnerCDO, ResultCDO);
		}
		return Result;
	}

	static void MarkPropertySetBySerialization(const UStruct* Struct, const void* StructData, const void* PropertyDataPtr)
	{
		if (const FSetProperty* ValuesSetByPropertyBagProperty = CastField<FSetProperty>(Struct->FindPropertyByName(Name_ValuesSetBySerialization)))
		{
			FScriptSetHelper ValuesSetByPropertyBag(ValuesSetByPropertyBagProperty, ValuesSetByPropertyBagProperty->ContainerPtrToValuePtr<void>(StructData));
			const int64 ValueOffset = static_cast<const uint8*>(PropertyDataPtr) - static_cast<const uint8*>(StructData);
			const int32 FoundIndex = ValuesSetByPropertyBag.FindElementIndex(&ValueOffset);
			if (FoundIndex == INDEX_NONE)
			{
				ValuesSetByPropertyBag.AddElement(&ValueOffset);
			}
		}
	}
	
	void MarkPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path)
	{
		const FPropertyPathNameSegment Segment = Path.GetSegment(Path.GetSegmentCount() - 1);

		// partially resolve path to find the parent struct and data pointer
		const UStruct* ParentStruct = Object->GetClass();
		const void* ParentData = Object;
		
		ResolvePropertyPathNameHelperParams Params {
            .Data = Object,
            .Path = Path,
            .CurPathIndex = 0,
			.EndPathIndex = Path.GetSegmentCount() - 1,
            .bAddIfNeeded = false
        };
		if (Params.CurPathIndex != Params.EndPathIndex)
		{
			if (!(ensure(ResolvePropertyPathNameHelper(Object->GetClass(), Params))))
            {
                return;
            }
            if (const FStructProperty* ParentAsStructProperty = CastField<FStructProperty>(Params.ResultProperty))
            {
                ParentData = Params.Data;
                ParentStruct = ParentAsStructProperty->Struct;
            }
            else
            {
            	// only mark properties set if they're in structs/classes
	            return;
            }
		}
		// resolve the last segment of the path
		Params.EndPathIndex += 1;
		if (ensure(ResolvePropertyPathNameHelper(ParentStruct, Params)))
        {
			MarkPropertySetBySerialization(ParentStruct, ParentData, Params.Data);
        }
	}

	bool WasPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path)
	{
		const FPropertyPathNameSegment Segment = Path.GetSegment(Path.GetSegmentCount() - 1);

		// partially resolve path to find the parent struct and data pointer
		const UStruct* ParentStruct = Object->GetClass();
		const void* ParentData = Object;
		
		ResolvePropertyPathNameHelperParams Params {
			.Data = Object,
			.Path = Path,
			.CurPathIndex = 0,
			.EndPathIndex = Path.GetSegmentCount() - 1,
			.bAddIfNeeded = false
		};
		if (Params.CurPathIndex != Params.EndPathIndex)
		{
			if (!(ensure(ResolvePropertyPathNameHelper(Object->GetClass(), Params))))
			{
				return false;
			}
			if (const FStructProperty* ParentAsStructProperty = CastField<FStructProperty>(Params.ResultProperty))
			{
				ParentData = Params.Data;
				ParentStruct = ParentAsStructProperty->Struct;
			}
		}

		// resolve the last segment of the path
		Params.EndPathIndex += 1;
		if (ensure(ResolvePropertyPathNameHelper(ParentStruct, Params)))
		{
			return WasPropertySetBySerialization(ParentStruct, ParentData, Params.ResultProperty, Segment.Index);
		}
		return false;
	}
	
	bool WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex)
	{
		if (ArrayIndex == INDEX_NONE)
		{
			ArrayIndex = 0;
		}
		if (const FSetProperty* ValuesSetByPropertyBagProperty = CastField<FSetProperty>(Struct->FindPropertyByName(Name_ValuesSetBySerialization)))
		{
			const uint8* PropertyDataPtr;
			if (ArrayIndex == INDEX_NONE || Property->IsA<FArrayProperty>() || Property->IsA<FMapProperty>() || Property->IsA<FSetProperty>())
			{
				PropertyDataPtr = Property->ContainerPtrToValuePtr<uint8>(StructData);
			}
			else
			{
				PropertyDataPtr = Property->ContainerPtrToValuePtr<uint8>(StructData, ArrayIndex);
			}

			const FScriptSetHelper ValuesSetByPropertyBag(ValuesSetByPropertyBagProperty, ValuesSetByPropertyBagProperty->ContainerPtrToValuePtr<void>(StructData));
			const int64 ValueOffset = PropertyDataPtr - static_cast<const uint8*>(StructData);
			return ValuesSetByPropertyBag.FindElementIndex(&ValueOffset) != INDEX_NONE;
		}
		return false;
	}
} // UE