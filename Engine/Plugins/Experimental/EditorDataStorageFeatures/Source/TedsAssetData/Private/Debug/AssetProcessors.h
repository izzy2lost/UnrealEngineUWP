// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "UObject/ObjectMacros.h"

#include "AssetProcessors.generated.h"

UCLASS()
class UTypedElementAssetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementAssetFactory() override = default;

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) override;

private:
};
