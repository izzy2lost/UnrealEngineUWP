// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementObjectReinstancingManager.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Memento/TypedElementMementoRowTypes.h"
#include "TypedElementDatabase.h"
#include "TypedElementDatabaseCompatibility.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"

UTypedElementObjectReinstancingManager::UTypedElementObjectReinstancingManager()
	: UObjectAddedObserverHandle(TypedElementInvalidQueryHandle)
	, MementoRowBaseTable(TypedElementInvalidTableHandle)
{
}

void UTypedElementObjectReinstancingManager::Initialize(UTypedElementDatabase& InDatabase, UTypedElementDatabaseCompatibility& InDataStorageCompatibility, UTypedElementMementoSystem& InMementoSystem)
{
	Database = &InDatabase;
	DataStorageCompatibility = &InDataStorageCompatibility;
	MementoSystem = &InMementoSystem;

	ReinstancingCallbackHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UTypedElementObjectReinstancingManager::HandleOnObjectsReinstanced);
	
	RegisterQueries();
}

void UTypedElementObjectReinstancingManager::Deinitialize()
{
	UnregisterQueries();
	
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(ReinstancingCallbackHandle);

	MementoSystem = nullptr;
	DataStorageCompatibility = nullptr;
	Database = nullptr;
}

void UTypedElementObjectReinstancingManager::RegisterQueries()
{
	using namespace TypedElementQueryBuilder;
	
	UObjectAddedObserverHandle = Database->RegisterQuery(
		Select(
			TEXT("Object Reinstancing: Added UObjectColumn"),
			FObserver::OnAdd<FTypedElementUObjectColumn>(),
			[this](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& UObjectColumn)
			{
				TypedElementRowHandle Memento;
				if (NewInstanceToMementoMap.RemoveAndCopyValue(UObjectColumn.Object.Get(), Memento))
				{
					Context.AddColumn<FTypedElementMementoReinstanceTarget>(Memento, FTypedElementMementoReinstanceTarget{.Target = Row});
				}
			})
			.Compile());

	ExternalObjectAddedObserverHandle = Database->RegisterQuery(
	Select(
		TEXT("Object Reinstancing: Added ExternalObjectColumn"),
		FObserver::OnAdd<FTypedElementExternalObjectColumn>(),
		[this](TypedElementDataStorage::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementExternalObjectColumn& ExternalObjectColumn)
		{
			TypedElementRowHandle Memento;
			if (NewInstanceToMementoMap.RemoveAndCopyValue(ExternalObjectColumn.Object, Memento))
			{
				Context.AddColumn<FTypedElementMementoReinstanceTarget>(Memento, FTypedElementMementoReinstanceTarget{.Target = Row});
			}
		})
		.Compile());
}

void UTypedElementObjectReinstancingManager::UnregisterQueries()
{
	// TODO: Observer queries cannot be unregistered in Mass
	// Database->UnregisterQuery(ExternalObjectAddedObserverHandle);
	// Database->UnregisterQuery(UObjectAddedObserverHandle);
}

void UTypedElementObjectReinstancingManager::HandleOnObjectsReinstanced(
	const FCoreUObjectDelegates::FReplacementObjectMap& ObjectReplacementMap)
{
	NewInstanceToMementoMap.Empty(ObjectReplacementMap.Num());
	
	for (FCoreUObjectDelegates::FReplacementObjectMap::TConstIterator Iter = ObjectReplacementMap.CreateConstIterator(); Iter; ++Iter)
	{
		const UObject* PreDeleteObject = Iter->Key;
		const UObject* NewInstanceObject = Iter->Value;
		
		TypedElementRowHandle ObjectRow = DataStorageCompatibility->FindRowWithCompatibleObject(PreDeleteObject);
		if (ObjectRow != TypedElementInvalidRowHandle)
		{
			TypedElementRowHandle Memento = MementoSystem->CreateMemento(Database.Get());
			MementoSystem->EnableMementoizeOnDelete(Database.Get(), ObjectRow, Memento);

			// Keep a temporary map of the new objects that will be created to the mementos that keep their state
			NewInstanceToMementoMap.Add(NewInstanceObject, Memento);
		}
	}
}
