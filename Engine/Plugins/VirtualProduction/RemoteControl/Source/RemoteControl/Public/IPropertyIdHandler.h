// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UnrealType.h"
#include "PropertyBag.h"

class FProperty;
class UObject;

class IPropertyIdHandler
{
public:
	virtual ~IPropertyIdHandler() = default;

	/** Whether this PropertyId Handler handles the given Property */
	virtual bool IsPropertySupported(const FProperty* InProperty) const = 0;

	/** Get the PropertyBag type of the given property */
	virtual EPropertyBagPropertyType GetPropertyType(const FProperty* InProperty) const = 0;

	/** Get the Property type FName */
	virtual FName GetPropertyTypeName(const FProperty* InProperty) const = 0;

	/** Get the Property type Object */
	virtual UObject* GetPropertyTypeObject(const FProperty* InProperty) const = 0;

	/** Get the Property default value (Used for UObject to create a new one for the ValueWidget) */
	virtual TObjectPtr<UObject> GetObjectPropertyDefaultValue(const FProperty* InProperty, const UClass* InClassToCreate) const = 0;

	/** Get the property inside the container if not inside any container return the property passed */
	const FProperty* GetPropertyInsideContainer(const FProperty* InProperty) const
	{
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
		{
			return ArrayProperty->Inner;
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(InProperty))
		{
			return MapProperty->ValueProp;
		}
		if (const FSetProperty* SetProperty = CastField<FSetProperty>(InProperty))
		{
			return SetProperty->ElementProp;
		}
		return InProperty;
	}
};
