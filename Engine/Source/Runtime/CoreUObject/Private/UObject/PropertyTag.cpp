// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyTag.h"

#include "Misc/AsciiSet.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"
#include "Serialization/SerializedPropertyScope.h"
#include "String/ParseTokens.h"
#include "String/Split.h"
#include "UObject/BlueprintsObjectVersion.h"
#include "UObject/DebugSerializationFlags.h"
#include "UObject/OverriddenPropertySet.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

/**
 * Enum flags that indicate that additional data was serialized for that property tag.
 * Registered flags should be serialized in ascending order.
 */
enum class EPropertyTagExtension : uint8
{
	NoExtension					= 0x00,
	ReserveForFutureUse			= 0x01, // Can be used to add a next group of extensions

	////////////////////////////////////////////////
	// First extension group
	OverridableInformation		= 0x02,

	//
	// Add more extensions for the first group here
	//
};

ENUM_CLASS_FLAGS(EPropertyTagExtension);

thread_local const FPropertyTag* FPropertyTagScope::CurrentPropertyTag = nullptr;

/*-----------------------------------------------------------------------------
FPropertyTag
-----------------------------------------------------------------------------*/
FPropertyTag::FPropertyTag()
	: OverrideOperation(EOverriddenPropertyOperation::None)
{
}

FPropertyTag::FPropertyTag(FProperty* Property, int32 InIndex, uint8* Value)
	: Prop(Property)
	, Name(Property->GetFName())
	, ArrayIndex(InIndex)
	, OverrideOperation(EOverriddenPropertyOperation::None)
{
	UE::FPropertyTypeNameBuilder TypeBuilder;
	Property->SaveTypeName(TypeBuilder);
	SetType(TypeBuilder.Build());

	Property->SaveToTag(*this);

	if (FBoolProperty* Bool = CastField<FBoolProperty>(Property))
	{
		BoolVal = Bool->GetPropertyValue(Value);
	}
}

void FPropertyTag::SetProperty(FProperty* Property)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Prop = Property;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FPropertyTag::SetPropertyGuid(const FGuid& InPropertyGuid)
{
	HasPropertyGuid = InPropertyGuid.IsValid();
	PropertyGuid = InPropertyGuid;
}

void FPropertyTag::SetType(UE::FPropertyTypeName InFullType)
{
	TypeName = InFullType;
	Type = TypeName.GetName();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (const EName* TagType = Type.ToEName(); TagType && Type.GetNumber() == NAME_NO_NUMBER_INTERNAL)
	{
		switch (*TagType)
		{
		case NAME_StructProperty:
			StructName = TypeName.GetParameterName(0);
			if (FName StructGuidName = TypeName.GetParameterName(1);
				StructGuidName.IsNone() || !FGuid::Parse(StructGuidName.ToString(), StructGuid))
			{
				StructGuid.Invalidate();
			}
			break;
		case NAME_ByteProperty:
		case NAME_EnumProperty:
			EnumName = TypeName.GetParameterName();
			break;
		case NAME_ArrayProperty:
		case NAME_OptionalProperty:
		case NAME_SetProperty:
			InnerType = TypeName.GetParameterName();
			break;
		case NAME_MapProperty:
			InnerType = TypeName.GetParameterName(0);
			ValueType = TypeName.GetParameterName(1);
			break;
		default:
			break;
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

static EPropertyTagExtension CalculatePropertyExtensionFlags(FArchive& UnderlyingArchive, FPropertyTag& Tag)
{
	EPropertyTagExtension PropertyTagExtensions = EPropertyTagExtension::NoExtension;

	if (UnderlyingArchive.IsSaving())
	{
		// OverridableInformation
		const FOverriddenPropertySet* OverriddenProperties = FOverridableSerializationLogic::GetOverriddenProperties();
		Tag.bExperimentalOverridableLogic = Tag.GetProperty()->HasAnyPropertyFlags(CPF_ExperimentalOverridableLogic);
		if (OverriddenProperties || Tag.bExperimentalOverridableLogic)
		{
			Tag.OverrideOperation = OverriddenProperties ? OverriddenProperties->GetOverriddenPropertyOperation(UnderlyingArchive.GetSerializedPropertyChain(), Tag.GetProperty()) : EOverriddenPropertyOperation::None;
			PropertyTagExtensions |= EPropertyTagExtension::OverridableInformation;
		}
	}

	return PropertyTagExtensions;
}

static void SerializePropertyExtensions(FStructuredArchive::FSlot Slot, EPropertyTagExtension PropertyTagExtensions, FPropertyTag& Tag)
{
	// Serialize tag extensions, consider doing an init function and context as `EClassSerializationControlExtension` if we add more extensions
	Slot << SA_ATTRIBUTE(TEXT("PropertyExtensions"), PropertyTagExtensions);

	// OverridableInformation
	if (EnumHasAnyFlags(PropertyTagExtensions, EPropertyTagExtension::OverridableInformation))
	{
		Slot << SA_ATTRIBUTE(TEXT("OverriddenPropertyOperation"), Tag.OverrideOperation);
		Slot << SA_ATTRIBUTE(TEXT("ExperimentalOverridableLogic"), Tag.bExperimentalOverridableLogic);
	}
}

static void ParsePathName(FStringView Path, UE::FPropertyTypeNameBuilder& Builder)
{
	FStringView OuterChain;
	FStringView ObjectName;
	if (UE::String::SplitLastOfAnyChar(Path, {TEXT('.'), TEXT(':')}, OuterChain, ObjectName))
	{
		Builder.AddName(FName(ObjectName));
		Builder.BeginParameters();
		UE::String::ParseTokensMultiple(OuterChain, {TEXT('.'), TEXT(':')}, [&Builder](FStringView Outer)
		{
			Builder.AddName(FName(Outer));
		});
		Builder.EndParameters();
	}
	else
	{
		Builder.AddName(FName(Path));
	}
}

// Serializer.
FArchive& operator<<(FArchive& Ar, FPropertyTag& Tag)
{
	FStructuredArchiveFromArchive(Ar).GetSlot() << Tag;
	return Ar;
}

void operator<<(FStructuredArchive::FSlot Slot, FPropertyTag& Tag)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	bool bIsTextFormat = UnderlyingArchive.IsTextFormat();

	const FPackageFileVersion Version = UnderlyingArchive.UEVer();

	check(!UnderlyingArchive.GetArchiveState().UseUnversionedPropertySerialization());
	checkf(!UnderlyingArchive.IsSaving() || Tag.GetProperty(), TEXT("FPropertyTag must be constructed with a valid property when used for saving data!"));

	const auto AddEnumPath = [](UE::FPropertyTypeNameBuilder& Builder, FName EnumName)
	{
		TStringBuilder<256> EnumPath(InPlace, EnumName);
		if (FAsciiSet::HasNone(EnumPath.ToView(), ".:"))
		{
			Builder.AddName(EnumName);
		}
		else
		{
			ParsePathName(EnumPath, Builder);
		}
	};

	if (!bIsTextFormat)
	{
		// Name.
		Slot << SA_ATTRIBUTE(TEXT("Name"), Tag.Name);
		if (Tag.Name.IsNone())
		{
			return;
		}
	}

	Slot << SA_ATTRIBUTE(TEXT("Type"), Tag.Type);

	if (UnderlyingArchive.IsSaving())
	{
		// remember the offset of the Size variable - UStruct::SerializeTaggedProperties will update it after the
		// property has been serialized.
		Tag.SizeOffset = UnderlyingArchive.Tell();
	}

	if (!bIsTextFormat)
	{
		Slot << SA_ATTRIBUTE(TEXT("Size"), Tag.Size);
		Slot << SA_ATTRIBUTE(TEXT("ArrayIndex"), Tag.ArrayIndex);
	}

	if (Tag.Type.GetNumber() == 0)
	{
		// Build Tag.TypeName from the partial type name that was saved in older versions.
		UE::FPropertyTypeNameBuilder TypeBuilder;
		TypeBuilder.AddName(Tag.Type);

		FNameEntryId TagType = Tag.Type.GetComparisonIndex();

		// only need to serialize this for structs
		if (TagType == NAME_StructProperty)
		{
			TypeBuilder.BeginParameters();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FName StructName = Tag.StructName;
			FGuid StructGuid = Tag.StructGuid;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			Slot << SA_ATTRIBUTE(TEXT("StructName"), StructName);
			TypeBuilder.AddName(StructName);
			if (Version >= VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG)
			{
				if (bIsTextFormat)
				{
					Slot << SA_OPTIONAL_ATTRIBUTE(TEXT("StructGuid"), StructGuid, FGuid());
				}
				else
				{
					Slot << SA_ATTRIBUTE(TEXT("StructGuid"), StructGuid);
				}
				if (StructGuid.IsValid())
				{
					TypeBuilder.AddGuid(StructGuid);
				}
			}
			TypeBuilder.EndParameters();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Tag.StructName = StructName;
			Tag.StructGuid = StructGuid;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		// only need to serialize this for bools
		else if (TagType == NAME_BoolProperty && !UnderlyingArchive.IsTextFormat())
		{
			if (UnderlyingArchive.IsSaving())
			{
				FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Tag.GetProperty());
				Slot << SA_ATTRIBUTE(TEXT("BoolVal"), Tag.BoolVal);
			}
			else
			{
				Slot << SA_ATTRIBUTE(TEXT("BoolVal"), Tag.BoolVal);
			}
		}
		// only need to serialize this for bytes/enums
		else if (TagType == NAME_ByteProperty)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FName EnumName = Tag.EnumName;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			if (UnderlyingArchive.IsTextFormat())
			{
				Slot << SA_OPTIONAL_ATTRIBUTE(TEXT("EnumName"), EnumName, NAME_None);
			}
			else
			{
				Slot << SA_ATTRIBUTE(TEXT("EnumName"), EnumName);
			}
			if (!EnumName.IsNone())
			{
				TypeBuilder.BeginParameters();
				AddEnumPath(TypeBuilder, EnumName);
				TypeBuilder.EndParameters();
			}
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Tag.EnumName = EnumName;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		else if (TagType == NAME_EnumProperty)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FName EnumName = Tag.EnumName;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			Slot << SA_ATTRIBUTE(TEXT("EnumName"), EnumName);
			TypeBuilder.BeginParameters();
			AddEnumPath(TypeBuilder, EnumName);
			TypeBuilder.AddName(NAME_ByteProperty);
			TypeBuilder.EndParameters();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Tag.EnumName = EnumName;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		// need to serialize the InnerType for arrays
		else if (TagType == NAME_ArrayProperty)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FName InnerType = Tag.InnerType;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			if (Version >= VAR_UE4_ARRAY_PROPERTY_INNER_TAGS)
			{
				Slot << SA_ATTRIBUTE(TEXT("InnerType"), InnerType);
			}
			TypeBuilder.BeginParameters();
			TypeBuilder.AddName(InnerType);
			TypeBuilder.EndParameters();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Tag.InnerType = InnerType;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		// need to serialize the InnerType for optionals.
		else if (TagType == NAME_OptionalProperty)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FName InnerType = Tag.InnerType;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			Slot << SA_ATTRIBUTE(TEXT("InnerType"), InnerType);
			TypeBuilder.BeginParameters();
			TypeBuilder.AddName(InnerType);
			TypeBuilder.EndParameters();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Tag.InnerType = InnerType;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		else if (Version >= VER_UE4_PROPERTY_TAG_SET_MAP_SUPPORT)
		{
			if (TagType == NAME_SetProperty)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				FName InnerType = Tag.InnerType;
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				Slot << SA_ATTRIBUTE(TEXT("InnerType"), InnerType);
				TypeBuilder.BeginParameters();
				TypeBuilder.AddName(InnerType);
				TypeBuilder.EndParameters();
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				Tag.InnerType = InnerType;
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
			else if (TagType == NAME_MapProperty)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				FName InnerType = Tag.InnerType;
				FName ValueType = Tag.ValueType;
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				Slot << SA_ATTRIBUTE(TEXT("InnerType"), InnerType);
				Slot << SA_ATTRIBUTE(TEXT("ValueType"), ValueType);
				TypeBuilder.BeginParameters();
				TypeBuilder.AddName(InnerType);
				TypeBuilder.AddName(ValueType);
				TypeBuilder.EndParameters();
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				Tag.InnerType = InnerType;
				Tag.ValueType = ValueType;
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		}

		if (UnderlyingArchive.IsLoading())
		{
			Tag.TypeName = TypeBuilder.Build();
		}
	}

	// Property tags to handle renamed blueprint properties effectively.
	if (Version >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG)
	{
		if (bIsTextFormat)
		{
			Slot << SA_OPTIONAL_ATTRIBUTE(TEXT("PropertyGuid"), Tag.PropertyGuid, FGuid());
			Tag.HasPropertyGuid = Tag.PropertyGuid.IsValid();
		}
		else
		{
			Slot << SA_ATTRIBUTE(TEXT("HasPropertyGuid"), Tag.HasPropertyGuid);
			if (Tag.HasPropertyGuid)
			{
				Slot << SA_ATTRIBUTE(TEXT("PropertyGuid"), Tag.PropertyGuid);
			}
		}
	}

	if (Version >= EUnrealEngineObjectUE5Version::PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION)
	{
		EPropertyTagExtension PropertyTagExtensions = CalculatePropertyExtensionFlags(UnderlyingArchive, Tag);
		SerializePropertyExtensions(Slot, PropertyTagExtensions, Tag);
	}
}

// Property serializer.
void FPropertyTag::SerializeTaggedProperty(FArchive& Ar, FProperty* Property, uint8* Value, const uint8* Defaults) const
{
	SerializeTaggedProperty(FStructuredArchiveFromArchive(Ar).GetSlot(), Property, Value, Defaults);
}

void FPropertyTag::SerializeTaggedProperty(FStructuredArchive::FSlot Slot, FProperty* Property, uint8* Value, const uint8* Defaults) const
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	const int64 StartOfProperty = UnderlyingArchive.Tell();

	if (!UnderlyingArchive.IsTextFormat() && Property->GetClass() == FBoolProperty::StaticClass())
	{
		// ensure that the property scope gets recorded for boolean properties even though the data is stored in the tag
		FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Property);
		UnderlyingArchive.Serialize(nullptr, 0); 

		FBoolProperty* Bool = (FBoolProperty*)Property;
		if (UnderlyingArchive.IsLoading())
		{
			Bool->SetPropertyValue(Value, BoolVal != 0);
		}

		Slot.EnterStream();	// Effectively discard
	}
	else
	{
#if WITH_EDITOR
		static const FName NAME_SerializeTaggedProperty = FName(TEXT("SerializeTaggedProperty"));
		FArchive::FScopeAddDebugData P(UnderlyingArchive, NAME_SerializeTaggedProperty);
		FArchive::FScopeAddDebugData A(UnderlyingArchive, Property->GetFName());
#endif
		FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Property);
		FPropertyTagScope CurrentPropertyTagScope(this);

		Property->SerializeItem(Slot, Value, Defaults);
	}

	// Ensure that we serialize what we expected to serialize.
	const int64 EndOfProperty = UnderlyingArchive.Tell();
	if (Size && (EndOfProperty - StartOfProperty != Size))
	{
		UE_LOG(LogClass, Error, TEXT("Failed loading tagged %s. Read %" INT64_FMT "B, expected %dB."), *GetFullNameSafe(Property), EndOfProperty - StartOfProperty, Size);
		UnderlyingArchive.Seek(StartOfProperty + Size);
		Property->ClearValue(Value);
	}
}
