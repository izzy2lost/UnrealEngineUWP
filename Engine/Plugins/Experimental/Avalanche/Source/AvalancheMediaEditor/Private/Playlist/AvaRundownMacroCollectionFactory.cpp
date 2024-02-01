// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvaRundownMacroCollectionFactory.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Playlist/AvaRundownMacroCollection.h"

UAvaRundownMacroCollectionFactory::UAvaRundownMacroCollectionFactory()
{
	SupportedClass = UAvaRundownMacroCollection::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;
}

uint32 UAvaRundownMacroCollectionFactory::GetMenuCategories() const
{
	const IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	return AssetTools.FindAdvancedAssetCategory("AvalancheCategory");
}

UObject* UAvaRundownMacroCollectionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags
	, UObject* Context, FFeedbackContext* Warn)
{
	if (ensure(SupportedClass == Class))
	{
		return NewObject<UAvaRundownMacroCollection>(InParent, Name, Flags);
	}
	return nullptr;
}

FString UAvaRundownMacroCollectionFactory::GetDefaultNewAssetName() const
{
	return TEXT("NewRundownMacros");
}
