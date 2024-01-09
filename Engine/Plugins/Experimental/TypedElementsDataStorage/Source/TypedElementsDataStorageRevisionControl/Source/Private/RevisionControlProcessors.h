// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "UObject/ObjectMacros.h"

#include "RevisionControlProcessors.generated.h"

UCLASS()
class UTypedElementRevisionControlFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementRevisionControlFactory() override = default;

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) const override;
	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const override;

private:
	void RegisterFetchUpdates(ITypedElementDataStorageInterface& DataStorage) const;
	mutable TypedElementDataStorage::QueryHandle FetchUpdates = TypedElementDataStorage::InvalidQueryHandle;
};
