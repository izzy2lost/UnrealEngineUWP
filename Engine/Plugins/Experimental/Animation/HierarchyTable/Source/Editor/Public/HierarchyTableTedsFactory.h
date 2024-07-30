// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "UObject/ObjectMacros.h"

#include "HierarchyTableTedsFactory.generated.h"

UCLASS()
class UTypedElementHierarchyTableFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementHierarchyTableFactory() override = default;

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) override;
	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
};
