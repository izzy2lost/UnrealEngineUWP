// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSessionPresetFactory.h"

#include "Assets/MultiUserReplicationSessionPreset.h"

#define LOCTEXT_NAMESPACE "ReplicationSessionPresetFactory"

UReplicationSessionPresetFactory::UReplicationSessionPresetFactory(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMultiUserReplicationSessionPreset::StaticClass();
}

bool UReplicationSessionPresetFactory::CanCreateNew() const
{
	return true;
}

FText UReplicationSessionPresetFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Replication Session Preset");
}

bool UReplicationSessionPresetFactory::ConfigureProperties()
{
	return true;
}

UObject* UReplicationSessionPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UMultiUserReplicationSessionPreset* Asset = NewObject<UMultiUserReplicationSessionPreset>(InParent, Name, Flags);
	return Asset;
}

#undef LOCTEXT_NAMESPACE