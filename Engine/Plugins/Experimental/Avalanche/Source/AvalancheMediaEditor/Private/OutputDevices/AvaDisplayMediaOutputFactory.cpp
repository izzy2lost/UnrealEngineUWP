// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaDisplayMediaOutputFactory.h"
#include "OutputDevices/AvaDisplayMediaOutput.h"
#include "AssetTypeCategories.h"

UAvaDisplayMediaOutputFactory::UAvaDisplayMediaOutputFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UAvaDisplayMediaOutput::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UAvaDisplayMediaOutputFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName
	, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UAvaDisplayMediaOutput>(InParent, InClass, InName, Flags);
}

uint32 UAvaDisplayMediaOutputFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}

bool UAvaDisplayMediaOutputFactory::ShouldShowInNewMenu() const
{
	return true;
}
