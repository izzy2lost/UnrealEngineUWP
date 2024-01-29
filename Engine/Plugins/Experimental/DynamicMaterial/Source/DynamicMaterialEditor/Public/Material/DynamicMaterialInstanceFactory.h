// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "Containers/UnrealString.h"
#include "DynamicMaterialInstanceFactory.generated.h"

class UDynamicMaterialInstance;

UCLASS(MinimalAPI, hidecategories = Object)
class UDynamicMaterialInstanceFactory : public UFactory
{
	GENERATED_BODY()

public:
	static const FString BaseDirectory;
	static const FString BaseName;

	UDynamicMaterialInstanceFactory();

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTip() const override;
};
