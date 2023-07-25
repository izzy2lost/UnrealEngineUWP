// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClientProfileAssetFactory.h"

#include "MultiUserReplicationClientProfileAsset.h"

#define LOCTEXT_NAMESPACE "UReplicationClientProfileAssetFactory"

UReplicationClientProfileAssetFactory::UReplicationClientProfileAssetFactory(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMultiUserReplicationClientProfileAsset::StaticClass();
}

bool UReplicationClientProfileAssetFactory::CanCreateNew() const
{
	return true;
}

FText UReplicationClientProfileAssetFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Client Profile");
}

bool UReplicationClientProfileAssetFactory::ConfigureProperties()
{
	return true;
}

UObject* UReplicationClientProfileAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UMultiUserReplicationClientProfileAsset* Asset = NewObject<UMultiUserReplicationClientProfileAsset>(InParent, Name, Flags);
	return Asset;
}

#undef LOCTEXT_NAMESPACE