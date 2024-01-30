// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "AvaAnimBindingReference.generated.h"

class UObject;

/**
 * DEPRECATED
 * Avalanche-side Bindings have been softly deprecated after moving UAvaSequence (previously UAvaAnimation)
 * to inherit from ULevelSequence.
 * This is softly deprecated so that Sequence duplications still copy over these bindings.
 */

/**
 * An external reference to an object, resolvable through an arbitrary context.
 * Bindings consist of the relative path to the object from a specific outer (the context)
 */
USTRUCT()
struct FAvaAnimBindingReference
{
	GENERATED_BODY();

	friend class UAvaSequence;

private:
	/** Object path relative to a passed in context object */
	UPROPERTY()
	FString ObjectPath;
};

/** Wrapping UStruct for the References Array */
USTRUCT()
struct FAvaAnimBindingReferenceArray
{
	GENERATED_BODY()

	friend class UAvaSequence;

private:
	UPROPERTY()
	TArray<FAvaAnimBindingReference> References;
};

/** UStruct that stores a one to many mapping from an Object Binding ID, to object references that pertain to that ID. */
USTRUCT()
struct FAvaAnimBindingReferences
{
	GENERATED_BODY()

	friend class UAvaSequence;

private:
	/** The map from object binding ID to an array of references that pertain to that ID */
	UPROPERTY()
	TMap<FGuid, FAvaAnimBindingReferenceArray> BindingIdToReferences;
};
