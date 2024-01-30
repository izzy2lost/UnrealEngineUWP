// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "AvaPlaylistFactory.generated.h"

UCLASS()
class AVALANCHEMEDIAEDITOR_API UAvaPlaylistFactory : public UFactory
{
	GENERATED_BODY()

public:

	UAvaPlaylistFactory();
	virtual ~UAvaPlaylistFactory() override;

protected:
	
	//~ Begin UFactory Interface
	virtual uint32 GetMenuCategories() const override;
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool DoesSupportClass(UClass* Class) override;
	virtual UClass* ResolveSupportedClass() override;
	//~ Begin UFactory Interface
};
