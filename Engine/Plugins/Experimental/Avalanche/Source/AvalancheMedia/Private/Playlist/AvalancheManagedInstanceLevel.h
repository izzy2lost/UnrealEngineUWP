// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Playlist/AvalancheManagedInstance.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

enum class EAvaMediaMapChangeType : uint8;

class FAvalancheManagedInstanceLevel : public FAvalancheManagedInstance
{
public:
	UE_AVA_INHERITS(FAvalancheManagedInstanceLevel, FAvalancheManagedInstance);

	FAvalancheManagedInstanceLevel(FAvalancheManagedInstanceCache* InParentCache, const FSoftObjectPath& InAssetPath);
	virtual ~FAvalancheManagedInstanceLevel() override;

	//~ Begin FGCObject
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject
	
	//~ Begin FAvalancheManagedInstance
	virtual bool IsValid() const override { return ManagedAvalancheLevel != nullptr;}
	virtual URemoteControlPreset* GetRemoteControlPreset() const override { return ManagedRemoteControlPreset;}
	virtual IAvaSceneInterface* GetSceneInterface() const override;
	//~ End FAvalancheManagedInstance

private:
	void DiscardSourceAvalancheLevel();
	void OnMapChangedEvent(UWorld* InWorld, EAvaMediaMapChangeType InEventType);

private:
	/** Keep a weak pointer to the loaded source level. This is used to unregister delegates if needed. */
	TWeakObjectPtr<UWorld> SourceAvalancheLevelWeak;
	
	TObjectPtr<UWorld> ManagedAvalancheLevel;
	TObjectPtr<UPackage> ManagedAvalancheLevelPackage;
	TObjectPtr<URemoteControlPreset> ManagedRemoteControlPreset;
};