// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/AvalancheManagedInstance.h"
#include "UObject/StrongObjectPtr.h"

class UAvalancheBlueprint;

/**
 * Container for a fully instanced Avalanche Blueprint.
 * The World is not fully initialized, but contains all loaded actors.
 * This is required for RemoteControl bindings to be resolved.
 */
class FAvalancheManagedInstanceBlueprint : public FAvalancheManagedInstance
{
public:
	UE_AVA_INHERITS(FAvalancheManagedInstanceBlueprint, FAvalancheManagedInstance);
	
	FAvalancheManagedInstanceBlueprint(FAvalancheManagedInstanceCache* InParentCache, const FSoftObjectPath& InAssetPath);
	virtual ~FAvalancheManagedInstanceBlueprint() override;

	//~ Begin FGCObject
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override;
	virtual IAvaSceneInterface* GetSceneInterface() const override;
	//~ End FGCObject

	//~ Begin FAvalancheManagedInstance
	virtual bool IsValid() const override;
	virtual URemoteControlPreset* GetRemoteControlPreset() const override;
	//~ End FAvalancheManagedInstance

private:
	TObjectPtr<UAvalancheBlueprint> SourceAvalancheBlueprint;
	TObjectPtr<UWorld> ManagedWorld;
	TObjectPtr<UAvalancheBlueprint> ManagedAvalancheBlueprint;
	TObjectPtr<UPackage> ManagedAvalancheBlueprintPackage;
};
