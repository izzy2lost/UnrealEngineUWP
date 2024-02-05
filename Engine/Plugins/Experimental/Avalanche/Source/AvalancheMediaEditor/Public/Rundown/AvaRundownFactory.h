// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "AvaRundownFactory.generated.h"

UCLASS()
class AVALANCHEMEDIAEDITOR_API UAvaRundownFactory : public UFactory
{
	GENERATED_BODY()

public:

	UAvaRundownFactory();
	virtual ~UAvaRundownFactory() override;

protected:
	
	//~ Begin UFactory Interface
	virtual uint32 GetMenuCategories() const override;
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool DoesSupportClass(UClass* Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual FString GetDefaultNewAssetName() const override;
	//~ Begin UFactory Interface
};
