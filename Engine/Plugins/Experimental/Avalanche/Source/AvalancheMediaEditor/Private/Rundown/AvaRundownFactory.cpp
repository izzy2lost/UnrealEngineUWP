// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/AvaRundownFactory.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Rundown/AvaRundown.h"

UAvaRundownFactory::UAvaRundownFactory()
{
	// Provide the factory with information about how to handle our asset
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAvaRundown::StaticClass();
}

UAvaRundownFactory::~UAvaRundownFactory()
{
}

uint32 UAvaRundownFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	return AssetTools.FindAdvancedAssetCategory("MotionDesignCategory");
}

bool UAvaRundownFactory::ConfigureProperties()
{
	return Super::ConfigureProperties();
}

bool UAvaRundownFactory::ShouldShowInNewMenu() const
{
	return Super::ShouldShowInNewMenu();
}

UObject* UAvaRundownFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
	, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, CallingContext);
}

UObject* UAvaRundownFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
	, UObject* Context, FFeedbackContext* Warn)
{
	UAvaRundown* Rundown = nullptr;
	if (ensure(SupportedClass == Class))
	{
		Rundown = NewObject<UAvaRundown>(InParent, Name, Flags);
	}
	return Rundown;
}

bool UAvaRundownFactory::DoesSupportClass(UClass* Class)
{
	return Class == UAvaRundown::StaticClass();
}

UClass* UAvaRundownFactory::ResolveSupportedClass()
{
	return Super::ResolveSupportedClass();
}

FString UAvaRundownFactory::GetDefaultNewAssetName() const
{
	return TEXT("NewRundown");
}

