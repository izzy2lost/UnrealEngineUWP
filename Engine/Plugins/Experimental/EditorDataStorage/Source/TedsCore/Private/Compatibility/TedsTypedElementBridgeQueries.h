// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/UObjectGlobals.h"

#include "TedsTypedElementBridgeQueries.generated.h"

class UTypedElementRegistry;

/**
 * This class is responsible for running queries that will ensure Typed Element Handles
 * are cleaned up when TEDS is shut down.
 */
UCLASS(Transient)
class UTypedElementBridgeDataStorageFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()
public:
	~UTypedElementBridgeDataStorageFactory() override = default;
	virtual uint8 GetOrder() const override;
	virtual void PreRegister(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void PreShutdown(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

	static bool IsEnabled();

private:
	void RegisterQuery_NewUObject(ITypedElementDataStorageInterface& DataStorage);
	void UnregisterQuery_NewUObject(ITypedElementDataStorageInterface& DataStorage);
	void CleanupTypedElementColumns(ITypedElementDataStorageInterface& DataStorage);
	void HandleOnEnabled(IConsoleVariable* CVar);
	
	TypedElementQueryHandle RemoveTypedElementRowHandleQuery = TypedElementDataStorage::InvalidQueryHandle;
	FDelegateHandle DebugEnabledDelegateHandle;
};
