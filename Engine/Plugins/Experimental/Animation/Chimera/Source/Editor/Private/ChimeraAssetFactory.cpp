// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChimeraAssetFactory.h"
#include "Chimera/ChimeraAsset.h"

#define LOCTEXT_NAMESPACE "ChimeraAssetFactory"

UChimeraAssetFactory::UChimeraAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UChimeraAsset::StaticClass();
}

UObject* UChimeraAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UChimeraAsset>(InParent, Class, Name, Flags);
}

FString UChimeraAssetFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewChimeraAsset"));
}

#undef LOCTEXT_NAMESPACE