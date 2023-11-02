// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertDefaultPropertySelection.h"
#include "ConcertDefaultSubobjectSelection.h"
#include "Templates/Function.h"
#include "ConcertReplicationEditorSettings.generated.h"

class UObject;
struct FConcertPropertyChain;
struct FReplicatedObjectInfo;

USTRUCT()
struct CONCERTCLIENTSHAREDSLATE_API FConcertReplicationEditorSettings
{
	GENERATED_BODY()
	
	/** Properties you want selected by default when you add a new replicated object in the editor */
	UPROPERTY(EditAnywhere, Config, Category = "Replication|Editor")
	TMap<FSoftClassPath, FConcertDefaultPropertySelection> DefaultPropertySelection;

	/**
	 * Subobjects you want selected by default when you add a new replicated object in the editor.
	 * For examples:
	 *  - Whenever you add a StaticMeshActor actor, you its static mesh component to be added automatically, too.
	 *  - Whenever you add a component, you want to add its child components, too.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Replication|Editor")
	TMap<FSoftClassPath, FConcertDefaultSubobjectSelection> DefaultSubobjectSelection;

	/** Reads DefaultPropertySelection and calls Callback for any default property selections based on the Class just added. */
	void AddDefaultPropertiesFromSettings(UClass& Class, TFunctionRef<void(FConcertPropertyChain&& Chain)> Callback) const;

	/** Reads DefaultComponentSelection and calls FurtherObjectsCallback on any further objects that should also be added. */
	void AddAdditionalObjectsFromSettings(const UObject& AddedObject, TFunctionRef<void(UObject&)> FurtherObjectsCallback) const;
};