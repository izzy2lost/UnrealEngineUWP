// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rundown/AvaRundownManagedInstance.h"
#include "UObject/StrongObjectPtr.h"

class UAvalancheBlueprint;

/**
 * Container for a fully instanced Motion Design Blueprint.
 * The World is not fully initialized, but contains all loaded actors.
 * This is required for RemoteControl bindings to be resolved.
 */
class FAvaRundownManagedInstanceBlueprint : public FAvaRundownManagedInstance
{
public:
	UE_AVA_INHERITS(FAvaRundownManagedInstanceBlueprint, FAvaRundownManagedInstance);
	
	FAvaRundownManagedInstanceBlueprint(FAvaRundownManagedInstanceCache* InParentCache, const FSoftObjectPath& InAssetPath);
	virtual ~FAvaRundownManagedInstanceBlueprint() override;

	//~ Begin FGCObject
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override;
	virtual IAvaSceneInterface* GetSceneInterface() const override;
	//~ End FGCObject

	//~ Begin FAvaRundownManagedInstance
	virtual bool IsValid() const override;
	virtual URemoteControlPreset* GetRemoteControlPreset() const override;
	//~ End FAvaRundownManagedInstance

private:
	TObjectPtr<UAvalancheBlueprint> SourceBlueprint;
	TObjectPtr<UWorld> ManagedWorld;
	TObjectPtr<UAvalancheBlueprint> ManagedBlueprint;
	TObjectPtr<UPackage> ManagedBlueprintPackage;
};
