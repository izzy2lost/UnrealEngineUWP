// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/UObjectGlobals.h"

#include "TedsTypedElementActorHandleFactory.generated.h"

class UTypedElementRegistry;

/**
 * This class is responsible for acquiring and registering Actor Typed Element Handles
 * with TEDS/
 */
UCLASS(Transient)
class UTypedElementActorHandleDataStorageFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()
public:

	virtual void PreRegister(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void PreShutdown(ITypedElementDataStorageInterface& DataStorage) override;
private:
	virtual void RegisterQuery_ActorHandlePopulate(ITypedElementDataStorageInterface& DataStorage);
	void HandleBridgeEnabled(bool bEnabled);

	FDelegateHandle BridgeEnableDelegateHandle;
	UE::Editor::DataStorage::QueryHandle ActorHandlePopulateQuery = UE::Editor::DataStorage::InvalidQueryHandle;
	UE::Editor::DataStorage::QueryHandle GetAllActorsQuery = UE::Editor::DataStorage::InvalidQueryHandle;
};


