// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "TypedElementCompatibilityColumns.generated.h"

class UObject;
class UWorld;

/**
 * Column containing a non-owning reference to a UObject.
 */
USTRUCT(meta = (DisplayName = "UObject reference"))
struct FTypedElementUObjectColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	TWeakObjectPtr<UObject> Object;
};

/**
 * Column containing a non-owning reference to an arbitrary object. It's strongly recommended
 * to also add a FTypedElementScriptStructTypeInfoColumn to make sure the type can be safely
 * recovered.
 */
USTRUCT(meta = (DisplayName = "External object reference"))
struct FTypedElementExternalObjectColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	void* Object;
};

/**
 * Tag to identify a row with an actor. If there's a FTypedElementUObjectColumn, the stored object will be
 * an actor if this tag is present.
 */
USTRUCT(meta = (DisplayName = "Actor"))
struct FTypedElementActorTag final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

/**
 * Column that stores a reference to the world.
 */
USTRUCT(meta = (DisplayName = "World"))
struct FTypedElementWorldColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	TWeakObjectPtr<UWorld> World;
};
