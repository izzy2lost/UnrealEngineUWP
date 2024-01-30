// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvaPlaylistFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Playlist/AvalanchePlaylist.h"

UAvaPlaylistFactory::UAvaPlaylistFactory()
{
	// Provide the factory with information about how to handle our asset
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAvalanchePlaylist::StaticClass();
}

UAvaPlaylistFactory::~UAvaPlaylistFactory()
{
}

uint32 UAvaPlaylistFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	return AssetTools.FindAdvancedAssetCategory("AvalancheCategory");
}

bool UAvaPlaylistFactory::ConfigureProperties()
{
	return Super::ConfigureProperties();
}

bool UAvaPlaylistFactory::ShouldShowInNewMenu() const
{
	return Super::ShouldShowInNewMenu();
}

UObject* UAvaPlaylistFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
	, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, CallingContext);
}

UObject* UAvaPlaylistFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
	, UObject* Context, FFeedbackContext* Warn)
{
	UAvalanchePlaylist* Playlist = nullptr;
	if (ensure(SupportedClass == Class))
	{
		Playlist = NewObject<UAvalanchePlaylist>(InParent, Name, Flags);
	}
	return Playlist;
}

bool UAvaPlaylistFactory::DoesSupportClass(UClass* Class)
{
	return Class == UAvalanchePlaylist::StaticClass();
}

UClass* UAvaPlaylistFactory::ResolveSupportedClass()
{
	return Super::ResolveSupportedClass();
}
