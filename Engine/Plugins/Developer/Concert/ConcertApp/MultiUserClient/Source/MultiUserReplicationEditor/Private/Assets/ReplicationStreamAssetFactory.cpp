// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationStreamAssetFactory.h"

#include "MultiUserReplicationStreamAsset.h"

#define LOCTEXT_NAMESPACE "ReplicationStreamAssetFactory"

UReplicationStreamAssetFactory::UReplicationStreamAssetFactory(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMultiUserReplicationStreamAsset::StaticClass();
}

bool UReplicationStreamAssetFactory::CanCreateNew() const
{
	return true;
}

FText UReplicationStreamAssetFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Replication Stream");
}

bool UReplicationStreamAssetFactory::ConfigureProperties()
{
	return true;
}

UObject* UReplicationStreamAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UMultiUserReplicationStreamAsset* Asset = NewObject<UMultiUserReplicationStreamAsset>(InParent, Name, Flags);
	return Asset;
}

#undef LOCTEXT_NAMESPACE