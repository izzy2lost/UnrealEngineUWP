// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorViewportProcessors.generated.h"

UCLASS()
class UActorViewportDataStorageFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UActorViewportDataStorageFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	void RegisterOutlineColorColumnToActor(ITypedElementDataStorageInterface& DataStorage);
	void RegisterOverlayColorColumnToActor(ITypedElementDataStorageInterface& DataStorage);
};
