// Copyright Epic Games, Inc. All Rights Reserved.

#include "StructPropertyIdHandler.h"

bool FStructPropertyIdHandler::IsPropertySupported(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FProperty* Property = GetPropertyInsideContainer(InProperty);
		return Property->GetClass()->IsChildOf(FStructProperty::StaticClass());
	}
	return false;
}

EPropertyBagPropertyType FStructPropertyIdHandler::GetPropertyType(const FProperty* InProperty) const
{
	return EPropertyBagPropertyType::Struct;
}

FName FStructPropertyIdHandler::GetPropertyTypeName(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FProperty* Property = GetPropertyInsideContainer(InProperty);
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct->GetFName() == NAME_LinearColor)
			{
				return NAME_Color;
			}
			return StructProperty->Struct->GetFName();
		}
	}
	return FName(TEXT(""));
}

UObject* FStructPropertyIdHandler::GetPropertyTypeObject(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FProperty* Property = GetPropertyInsideContainer(InProperty);
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct->GetFName() == NAME_LinearColor)
			{
				return TBaseStructure<FColor>::Get();
			}
			return StructProperty->Struct;
		}
	}
	return nullptr;
}

TObjectPtr<UObject> FStructPropertyIdHandler::GetObjectPropertyDefaultValue(const FProperty* InProperty, const UClass* InClassToCreate) const
{
	return nullptr;
}
