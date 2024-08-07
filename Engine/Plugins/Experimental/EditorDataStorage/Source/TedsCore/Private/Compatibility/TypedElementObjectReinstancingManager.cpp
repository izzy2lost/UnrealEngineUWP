// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementObjectReinstancingManager.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Memento/TypedElementMementoRowTypes.h"
#include "TypedElementDatabase.h"
#include "TypedElementDatabaseCompatibility.h"
#include "TypedElementDatabaseEnvironment.h"

DECLARE_LOG_CATEGORY_CLASS(LogTedsObjectReinstancing, Log, Log)

UTypedElementObjectReinstancingManager::UTypedElementObjectReinstancingManager()
	: MementoRowBaseTable(TypedElementInvalidTableHandle)
{
}

void UTypedElementObjectReinstancingManager::Initialize(UTypedElementDatabase& InDatabase, UTypedElementDatabaseCompatibility& InDataStorageCompatibility)
{
	using namespace TypedElementDataStorage;
	using namespace UE::Editor::DataStorage;

	Database = &InDatabase;
	DataStorageCompatibility = &InDataStorageCompatibility;
	
	UpdateCompletedCallbackHandle = 
		Database->OnUpdateCompleted().AddUObject(this, &UTypedElementObjectReinstancingManager::UpdateCompleted);
	ReinstancingCallbackHandle = 
		FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UTypedElementObjectReinstancingManager::HandleOnObjectsReinstanced);
	ObjectRemovedCallbackHandle = DataStorageCompatibility->RegisterObjectRemovedCallback(
		[this](const void* Object, const FObjectTypeInfo& TypeInfo, RowHandle Row)
		{
			HandleOnObjectPreRemoved(Object, TypeInfo, Row);
		});
}

void UTypedElementObjectReinstancingManager::Deinitialize()
{
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(ReinstancingCallbackHandle);
	DataStorageCompatibility->UnregisterObjectRemovedCallback(ObjectRemovedCallbackHandle);
	Database->OnUpdateCompleted().Remove(UpdateCompletedCallbackHandle);

	DataStorageCompatibility = nullptr;
	Database = nullptr;
}

void UTypedElementObjectReinstancingManager::UpdateCompleted()
{
	using namespace TypedElementDataStorage;

	UTypedElementMementoSystem& MementoSystem = Database->GetEnvironment()->GetMementoSystem();
	for (TMap<const void*, RowHandle>::TConstIterator It = OldObjectToMementoMap.CreateConstIterator(); It; ++It)
	{
		MementoSystem.DestroyMemento(It.Value());
	}
	OldObjectToMementoMap.Reset();
}

void UTypedElementObjectReinstancingManager::HandleOnObjectPreRemoved(
	const void* Object, 
	const UE::Editor::DataStorage::FObjectTypeInfo& TypeInfo, 
	TypedElementDataStorage::RowHandle ObjectRow)
{
	// This is the chance to record the old object to memento
	TypedElementDataStorage::RowHandle Memento = Database->GetEnvironment()->GetMementoSystem().CreateMemento(ObjectRow);
	OldObjectToMementoMap.Add(Object, Memento);
}

void UTypedElementObjectReinstancingManager::HandleOnObjectsReinstanced(
	const FCoreUObjectDelegates::FReplacementObjectMap& ObjectReplacementMap)
{
	ITypedElementDataStorageInterface* Interface = Database;
	for (FCoreUObjectDelegates::FReplacementObjectMap::TConstIterator Iter = ObjectReplacementMap.CreateConstIterator(); Iter; ++Iter)
	{
		const void* PreDeleteObject = Iter->Key;
		if (const TypedElementRowHandle* MementoRowPtr = OldObjectToMementoMap.Find(PreDeleteObject))
		{
			TypedElementRowHandle Memento = *MementoRowPtr;
			
			UObject* NewInstanceObject = Iter->Value;
			if (NewInstanceObject == nullptr)
			{
				continue;
			}
			
			TypedElementRowHandle NewObjectRow = DataStorageCompatibility->FindRowWithCompatibleObjectExplicit(NewInstanceObject);
			// Do the addition only if there's a recorded memento. Having a memento implies the object was previously registered and there's
			// still an interest in it. Any other objects can therefore be ignored.
			if (!Interface->IsRowAvailable(NewObjectRow))
			{
				NewObjectRow = DataStorageCompatibility->AddCompatibleObjectExplicit(NewInstanceObject);
			}

			// Kick off re-instantiation of NewObjectRow from the Memento
			Database->GetEnvironment()->GetMementoSystem().RestoreMemento(Memento, NewObjectRow);
		}
	}
}
