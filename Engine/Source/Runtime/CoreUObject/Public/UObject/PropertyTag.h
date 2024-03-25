// Copyright Epic Games, Inc. All Rights Reserved.

/*-----------------------------------------------------------------------------
	FPropertyTag.
-----------------------------------------------------------------------------*/

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "Serialization/StructuredArchive.h"
#include "UObject/NameTypes.h"
#include "UObject/PropertyTypeName.h"

enum class EOverriddenPropertyOperation : uint8;
class FArchive;
class FProperty;

/**
 *  A tag describing a class property, to aid in serialization.
 */
struct FPropertyTag
{
	// Transient.
	FProperty* Prop = nullptr;

	// Variables.
private:
	UE::FPropertyTypeName TypeName;

public:
	FName	Type;		// Type of property
	FName	Name;		// Name of property.
	FName	StructName;	// Struct name if FStructProperty.
	FName	EnumName;	// Enum name if FByteProperty or FEnumProperty
	FName	InnerType;	// Inner type if FArrayProperty, FSetProperty, FMapProperty, or OptionalProperty
	FName	ValueType;	// Value type if UMapPropery
	int32	Size = 0;   // Property size.
	int32	ArrayIndex = INDEX_NONE; // Index if an array; else 0.
	int64	SizeOffset = INDEX_NONE; // location in stream of tag size member
	FGuid	StructGuid;
	FGuid	PropertyGuid;
	uint8	HasPropertyGuid = 0;
	uint8	BoolVal = 0;// a boolean property's value (never need to serialize data for bool properties except here)
	EOverriddenPropertyOperation OverrideOperation; // Overridable serialization state reconstruction 
	bool	bExperimentalOverridableLogic = false; // Remember if property had CPF_ExperimentalOverridableLogic when saved

	// Constructors.
	FPropertyTag();
	FPropertyTag(FProperty* Property, int32 InIndex, uint8* Value);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPropertyTag(FPropertyTag&&) = default;
	FPropertyTag(const FPropertyTag&) = default;
	FPropertyTag& operator=(FPropertyTag&&) = default;
	FPropertyTag& operator=(const FPropertyTag&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	inline FProperty* GetProperty() const
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Prop;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	void SetProperty(FProperty* Property);
	void SetPropertyGuid(const FGuid& InPropertyGuid);

	inline UE::FPropertyTypeName GetType() const { return TypeName; }
	void SetType(UE::FPropertyTypeName TypeName);

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar, FPropertyTag& Tag);
	friend void operator<<(FStructuredArchive::FSlot Slot, FPropertyTag& Tag);

	// Property serializer.
	void SerializeTaggedProperty(FArchive& Ar, FProperty* Property, uint8* Value, const uint8* Defaults) const;
	void SerializeTaggedProperty(FStructuredArchive::FSlot Slot, FProperty* Property, uint8* Value, const uint8* Defaults) const;
};

struct UE_INTERNAL FPropertyTagScope
{
	FPropertyTagScope(const FPropertyTag* InCurrentPropertyTag)
	: PropertyTagToRestore(CurrentPropertyTag)
	{
		CurrentPropertyTag = InCurrentPropertyTag;
	}

	~FPropertyTagScope()
	{
		CurrentPropertyTag = PropertyTagToRestore;
	}

	static FORCEINLINE const FPropertyTag* GetCurrentPropertyTag()
	{
		return CurrentPropertyTag;
	}
private:
	const FPropertyTag* PropertyTagToRestore;

	static thread_local const FPropertyTag* CurrentPropertyTag;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
