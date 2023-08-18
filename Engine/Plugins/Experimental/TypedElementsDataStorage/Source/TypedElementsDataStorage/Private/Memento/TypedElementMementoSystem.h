// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"

#include "TypedElementMementoSystem.generated.h"


UCLASS()
class UTypedElementMementoSystemFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const override;
	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) const override;

	TypedElementTableHandle GetUnpopulatedMementoTable() const;
private:
	mutable TypedElementTableHandle MementoRowBaseTable;
};

inline TypedElementTableHandle UTypedElementMementoSystemFactory::GetUnpopulatedMementoTable() const
{
	return MementoRowBaseTable;
}
