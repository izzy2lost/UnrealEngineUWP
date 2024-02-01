// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvaPlaybackFactory.h"
#include "AssetToolsModule.h"
#include "ClassViewerFilter.h"
#include "IAssetTools.h"
#include "Playback/AvalanchePlayback.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackFactory"

UAvaPlaybackFactory::UAvaPlaybackFactory()
{
	// Provide the factory with information about how to handle our asset
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAvalanchePlayback::StaticClass();
}

UAvaPlaybackFactory::~UAvaPlaybackFactory()
{
}

uint32 UAvaPlaybackFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	return AssetTools.FindAdvancedAssetCategory("AvalancheCategory");
}

bool UAvaPlaybackFactory::ConfigureProperties()
{
	return Super::ConfigureProperties();
}

bool UAvaPlaybackFactory::ShouldShowInNewMenu() const
{
	return Super::ShouldShowInNewMenu();
}

UObject* UAvaPlaybackFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
  , UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, CallingContext);
}

UObject* UAvaPlaybackFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
  , UObject* Context, FFeedbackContext* Warn)
{
	UAvalanchePlayback* Playback = nullptr;
	if (ensure(SupportedClass == Class))
	{
		Playback = NewObject<UAvalanchePlayback>(InParent, Name, Flags);
	}
	return Playback;
}

bool UAvaPlaybackFactory::DoesSupportClass(UClass* Class)
{
	return Class == UAvalanchePlayback::StaticClass();
}

UClass* UAvaPlaybackFactory::ResolveSupportedClass()
{
	return Super::ResolveSupportedClass();
}

FString UAvaPlaybackFactory::GetDefaultNewAssetName() const
{
	return TEXT("NewPlaybackGraph");
}

#undef LOCTEXT_NAMESPACE
