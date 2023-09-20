// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertDefaultPropertySelection.h"
#include "ConcertDefaultSubobjectSelection.h"
#include "ConcertReplicationEditorSettings.generated.h"

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

	/** Reads DefaultPropertySelection and applies any default property selections to Info based on the Class just added. */
	void AddDefaultPropertiesFromSettings(FReplicatedObjectInfo& Info, UClass& Class) const;

	/** Reads DefaultComponentSelection and calls FurtherObjectsCallback on any further objects that should also be added. */
	void AddAdditionalObjectsFromSettings(UObject& AddedObject, TFunctionRef<void(UObject&)> FurtherObjectsCallback) const;
};