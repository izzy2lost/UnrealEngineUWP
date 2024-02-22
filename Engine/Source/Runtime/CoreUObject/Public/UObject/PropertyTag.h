// Copyright Epic Games, Inc. All Rights Reserved.

/*-----------------------------------------------------------------------------
	FPropertyTag.
-----------------------------------------------------------------------------*/

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "Serialization/StructuredArchive.h"
#include "UObject/NameTypes.h"

enum class EOverriddenPropertyOperation : uint8;
class FArchive;
class FProperty;

/**
 *  Enum flags that indicate that additional data was serialized for that property tag
 *	Registered flag should be serialized in ascending order 
 */
enum class EPropertyTagExtension : uint8
{
	NoExtension					= 0x00,
	ReserveForFutureUse			= 0x01, // Can be use to add a next group of extension

	////////////////////////////////////////////////
	// First extension group
	OverridableInformation		= 0x02,

	//
	// Add more extension for the first group here
	//
};
ENUM_CLASS_FLAGS(EPropertyTagExtension);

/**
 *  A tag describing a class property, to aid in serialization.
 */
struct FPropertyTag
{
	// Transient.
	FProperty* Prop = nullptr;

	// Variables.
	FName	Type;		// Type of property
	uint8	BoolVal = 0;// a boolean property's value (never need to serialize data for bool properties except here)
	FName	Name;		// Name of property.
	FName	StructName;	// Struct name if FStructProperty.
	FName	EnumName;	// Enum name if FByteProperty or FEnumProperty
	FName	InnerType;	// Inner type if FArrayProperty, FSetProperty, or FMapProperty
	FName	ValueType;	// Value type if UMapPropery
	int32	Size = 0;   // Property size.
	int32	ArrayIndex = INDEX_NONE; // Index if an array; else 0.
	int64	SizeOffset = INDEX_NONE; // location in stream of tag size member
	FGuid	StructGuid;
	uint8	HasPropertyGuid = 0;
	FGuid	PropertyGuid;
	EOverriddenPropertyOperation OverrideOperation; // Overridable serialization state reconstruction 
	bool	bExperimentalOverridableLogic = false; // Remember if property had CPF_ExperimentalOverridableLogic when saved

	// Constructors.
	FPropertyTag();
	UE_INTERNAL COREUOBJECT_API FPropertyTag( FArchive& InSaveAr, FProperty* Property, int32 InIndex, uint8* Value, const uint8* Defaults );

	// Set optional property guid
	void SetPropertyGuid(const FGuid& InPropertyGuid);

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPropertyTag& Tag );
	friend void operator<<(FStructuredArchive::FSlot Slot, FPropertyTag& Tag);

	// Property serializer.
	void SerializeTaggedProperty( FArchive& Ar, FProperty* Property, uint8* Value, const uint8* Defaults ) const;
	UE_INTERNAL COREUOBJECT_API void SerializeTaggedProperty(FStructuredArchive::FSlot Slot, FProperty* Property, uint8* Value, const uint8* Defaults) const;
};

struct FPropertyTagScope
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
