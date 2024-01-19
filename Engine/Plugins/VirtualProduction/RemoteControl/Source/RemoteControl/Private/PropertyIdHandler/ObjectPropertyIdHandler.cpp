// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectPropertyIdHandler.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

bool FObjectPropertyIdHandler::IsPropertySupported(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FProperty* Property = GetPropertyInsideContainer(InProperty);
		return Property->GetClass()->IsChildOf(FObjectProperty::StaticClass());
	}
	return false;
}

EPropertyBagPropertyType FObjectPropertyIdHandler::GetPropertyType(const FProperty* InProperty) const
{
	return EPropertyBagPropertyType::Object;
}

FName FObjectPropertyIdHandler::GetPropertyTypeName(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FProperty* Property = GetPropertyInsideContainer(InProperty);
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			return ObjectProperty->PropertyClass->GetFName();
		}
	}
	return FName(TEXT(""));
}

UObject* FObjectPropertyIdHandler::GetPropertyTypeObject(const FProperty* InProperty) const
{
	return nullptr;
}

TObjectPtr<UObject> FObjectPropertyIdHandler::GetObjectPropertyDefaultValue(const FProperty* InProperty, const UClass* InClassToCreate) const
{
	if (InClassToCreate)
	{
		if (InClassToCreate->IsChildOf(UMaterialInterface::StaticClass()))
    	{
    		return UMaterial::GetDefaultMaterial(MD_Surface);
    	}

		if (InClassToCreate->HasAnyClassFlags(CLASS_Abstract))
		{
			// for now skip any AbstractClass
			return nullptr;
		}

		return NewObject<UObject>(GetTransientPackage(), InClassToCreate);
	}
	return nullptr;
}
