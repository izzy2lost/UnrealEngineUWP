// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"

#include "AssetProcessors.generated.h"

class ITypedElementDataStorageInterface;

UCLASS()
class UTedsAssetDataFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTedsAssetDataFactory() override = default;

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) override;

private:
};
