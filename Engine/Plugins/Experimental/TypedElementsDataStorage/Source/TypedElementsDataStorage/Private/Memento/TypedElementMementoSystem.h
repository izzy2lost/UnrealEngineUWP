// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"

#include "TypedElementMementoSystem.generated.h"

class ITypedElementDataStorageInterface;
class IConsoleVariable;
struct FTypedElementDatabaseCompatibilityObjectTypeInfo;

UCLASS()
class UTypedElementMementoSystemFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const override;
	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) const override;
	void RegisterRegistrationFilters(ITypedElementDataStorageCompatibilityInterface& DataStorageCompatibility) const override;
private:

	void RegisterWithCompatibilityLayer(ITypedElementDataStorageCompatibilityInterface& DataStorageCompatibility);
	void HandleObjectAddedToCompatibility(ITypedElementDataStorageInterface* Storage, const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle Row);
	
	mutable TypedElementTableHandle MementoRowBaseTable;
	FDelegateHandle ObjectAddedDelegateHandle;
};
