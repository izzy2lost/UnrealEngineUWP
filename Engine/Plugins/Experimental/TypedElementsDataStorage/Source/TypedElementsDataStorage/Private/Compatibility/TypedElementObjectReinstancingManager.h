// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "Memento/TypedElementMementoSystem.h"
#include "UObject/UObjectGlobals.h"

#include "TypedElementObjectReinstancingManager.generated.h"

class ITypedElementDataStorageCompatibilityInterface;
class ITypedElementDataStorageInterface;
class UTypedElementDatabaseCompatibility;
class UTypedElementMementoSystem;

UCLASS(Transient)
class UTypedElementObjectReinstancingManager : public UObject
{
	GENERATED_BODY()
public:
	UTypedElementObjectReinstancingManager();

	void Initialize(UTypedElementDatabase& InDatabase, UTypedElementDatabaseCompatibility& InDataStorageCompatibility, UTypedElementMementoSystem& InMementoSystem);
	void Deinitialize();

private:
	void RegisterQueries();
	void UnregisterQueries();
	void HandleOnObjectsReinstanced(const FCoreUObjectDelegates::FReplacementObjectMap& ObjectReplacementMap);

	TObjectPtr<UTypedElementDatabase> Database = nullptr;
	TObjectPtr<UTypedElementDatabaseCompatibility> DataStorageCompatibility = nullptr;
	TObjectPtr<UTypedElementMementoSystem> MementoSystem = nullptr;
	
	// Maps objects that are about to be created to the memento that will be used to reinstance them
	TMap<const void*, TypedElementRowHandle> NewInstanceToMementoMap;

	TypedElementQueryHandle UObjectAddedObserverHandle;
	TypedElementQueryHandle ExternalObjectAddedObserverHandle;
	
	TypedElementTableHandle MementoRowBaseTable;
	FDelegateHandle ReinstancingCallbackHandle;
};
