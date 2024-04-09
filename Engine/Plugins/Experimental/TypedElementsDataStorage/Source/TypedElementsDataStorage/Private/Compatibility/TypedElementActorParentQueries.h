// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorParentQueries.generated.h"

UCLASS()
class UTypedElementActorParentFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementActorParentFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	/**
	 * Checks rows with actors that don't have a parent column yet if one needs to be added whenever
	 * the row is marked for updates.
	 */
	void RegisterAddParentColumn(ITypedElementDataStorageInterface& DataStorage) const;
	/**
	 * Updates the parent column with the parent from the actor or removes it if there's no parent associated
	 * with the actor anymore.
	 */
	void RegisterUpdateOrRemoveParentColumn(ITypedElementDataStorageInterface& DataStorage) const;
};
