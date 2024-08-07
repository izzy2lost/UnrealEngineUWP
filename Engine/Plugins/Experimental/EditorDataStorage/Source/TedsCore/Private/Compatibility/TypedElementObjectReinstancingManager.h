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

namespace UE::Editor::DataStorage
{
	struct FObjectTypeInfo;
}

UCLASS(Transient)
class UTypedElementObjectReinstancingManager : public UObject
{
	GENERATED_BODY()
public:
	UTypedElementObjectReinstancingManager();

	void Initialize(UTypedElementDatabase& InDatabase, UTypedElementDatabaseCompatibility& InDataStorageCompatibility);
	void Deinitialize();

private:
	void UpdateCompleted();
	void HandleOnObjectPreRemoved(
		const void* Object, 
		const UE::Editor::DataStorage::FObjectTypeInfo& TypeInfo, 
		TypedElementDataStorage::RowHandle ObjectRow);
	void HandleOnObjectsReinstanced(const FCoreUObjectDelegates::FReplacementObjectMap& ObjectReplacementMap);

	UPROPERTY()
	TObjectPtr<UTypedElementDatabase> Database = nullptr;
	UPROPERTY()
	TObjectPtr<UTypedElementDatabaseCompatibility> DataStorageCompatibility = nullptr;
	
	// Reverse lookup that holds all populated mementos for recently deleted objects
	// Entry removed when the memento is removed
	TMap<const void*, TypedElementDataStorage::RowHandle> OldObjectToMementoMap;
	
	TypedElementDataStorage::TableHandle MementoRowBaseTable;
	FDelegateHandle UpdateCompletedCallbackHandle;
	FDelegateHandle ReinstancingCallbackHandle;
	FDelegateHandle ObjectRemovedCallbackHandle;
};
