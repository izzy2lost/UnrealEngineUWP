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

	void RegisterTables(ITypedElementDataStorageInterface& DataStorage) override;
	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	void RegisterFetchUpdates(ITypedElementDataStorageInterface& DataStorage);
	void RegisterApplyOverlays(ITypedElementDataStorageInterface& DataStorage);
	void RegisterRemoveOverlays(ITypedElementDataStorageInterface& DataStorage);
	TypedElementDataStorage::QueryHandle FetchUpdates = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle ApplyNewOverlays = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle ChangeOverlay = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle ApplyOverlaysObjectToSCC = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle RemoveOverlays = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle FlushPackageUpdates = TypedElementDataStorage::InvalidQueryHandle;
};
