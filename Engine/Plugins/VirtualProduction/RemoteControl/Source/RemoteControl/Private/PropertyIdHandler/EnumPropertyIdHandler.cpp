// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnumPropertyIdHandler.h"

bool FEnumPropertyIdHandler::IsPropertySupported(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FProperty* Property = GetPropertyInsideContainer(InProperty);
		return Property->GetClass()->IsChildOf(FEnumProperty::StaticClass());
	}
	return false;
}

EPropertyBagPropertyType FEnumPropertyIdHandler::GetPropertyType(const FProperty* InProperty) const
{
	return EPropertyBagPropertyType::Enum;
}

FName FEnumPropertyIdHandler::GetPropertyTypeName(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FProperty* Property = GetPropertyInsideContainer(InProperty);
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum()->GetFName();
		}
	}
	return FName(TEXT(""));
}

UObject* FEnumPropertyIdHandler::GetPropertyTypeObject(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FProperty* Property = GetPropertyInsideContainer(InProperty);
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum();
		}
	}
	return nullptr;
}

TObjectPtr<UObject> FEnumPropertyIdHandler::GetObjectPropertyDefaultValue(const FProperty* InProperty, const UClass* InClassToCreate) const
{
	return nullptr;
}
