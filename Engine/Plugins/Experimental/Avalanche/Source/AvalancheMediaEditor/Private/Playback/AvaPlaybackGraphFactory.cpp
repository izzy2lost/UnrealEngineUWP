// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvaPlaybackGraphFactory.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Playback/AvaPlaybackGraph.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackGraphFactory"

UAvaPlaybackGraphFactory::UAvaPlaybackGraphFactory()
{
	// Provide the factory with information about how to handle our asset
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAvaPlaybackGraph::StaticClass();
}

UAvaPlaybackGraphFactory::~UAvaPlaybackGraphFactory()
{
}

uint32 UAvaPlaybackGraphFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	return AssetTools.FindAdvancedAssetCategory("MotionDesignCategory");
}

bool UAvaPlaybackGraphFactory::ConfigureProperties()
{
	return Super::ConfigureProperties();
}

bool UAvaPlaybackGraphFactory::ShouldShowInNewMenu() const
{
	return Super::ShouldShowInNewMenu();
}

UObject* UAvaPlaybackGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
  , UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, CallingContext);
}

UObject* UAvaPlaybackGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
  , UObject* Context, FFeedbackContext* Warn)
{
	UAvaPlaybackGraph* Playback = nullptr;
	if (ensure(SupportedClass == Class))
	{
		Playback = NewObject<UAvaPlaybackGraph>(InParent, Name, Flags);
	}
	return Playback;
}

bool UAvaPlaybackGraphFactory::DoesSupportClass(UClass* Class)
{
	return Class == UAvaPlaybackGraph::StaticClass();
}

UClass* UAvaPlaybackGraphFactory::ResolveSupportedClass()
{
	return Super::ResolveSupportedClass();
}

FString UAvaPlaybackGraphFactory::GetDefaultNewAssetName() const
{
	return TEXT("NewPlaybackGraph");
}

#undef LOCTEXT_NAMESPACE
