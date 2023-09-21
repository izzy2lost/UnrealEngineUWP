// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyIdHandler.h"

class FBasePropertyIdHandler : public IPropertyIdHandler
{
public:
	//~ Begin IPropertyIdHandler
	virtual bool IsPropertySupported(const FProperty* InProperty) const override;
	virtual EPropertyBagPropertyType GetPropertyType(const FProperty* InProperty) const override;
	virtual FName GetPropertyTypeName(const FProperty* InProperty) const override;
	virtual UObject* GetPropertyTypeObject(const FProperty* InProperty) const override;
	virtual TObjectPtr<UObject> GetObjectPropertyDefaultValue(const FProperty* InProperty, const UClass* InClassToCreate) const override;
	//~ End IPropertyIdHandler
};
