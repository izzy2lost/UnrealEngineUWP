// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementObjectReinstancingManager.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Memento/TypedElementMementoRowTypes.h"
#include "TypedElementDatabase.h"
#include "TypedElementDatabaseCompatibility.h"
#include "Memento/TypedElementMementoInterface.h"

DECLARE_LOG_CATEGORY_CLASS(LogTedsObjectReinstancing, Log, Log)

UTypedElementObjectReinstancingManager::UTypedElementObjectReinstancingManager()
	: MementoRowBaseTable(TypedElementInvalidTableHandle)
{
}

void UTypedElementObjectReinstancingManager::Initialize(UTypedElementDatabase& InDatabase, UTypedElementDatabaseCompatibility& InDataStorageCompatibility, UTypedElementMementoSystem& InMementoSystem)
{
	Database = &InDatabase;
	DataStorageCompatibility = &InDataStorageCompatibility;
	MementoSystem = &InMementoSystem;

	ReinstancingCallbackHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UTypedElementObjectReinstancingManager::HandleOnObjectsReinstanced);
	ObjectRemovedCallbackHandle = DataStorageCompatibility->RegisterObjectRemovedCallback([this](const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle Row)
	{
		HandleOnObjectPreRemoved(Object, TypeInfo, Row);
	});
	
	RegisterQueries();
}

void UTypedElementObjectReinstancingManager::Deinitialize()
{
	UnregisterQueries();
	
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(ReinstancingCallbackHandle);
	DataStorageCompatibility->UnregisterObjectRemovedCallback(ObjectRemovedCallbackHandle);

	MementoSystem = nullptr;
	DataStorageCompatibility = nullptr;
	Database = nullptr;
}

void UTypedElementObjectReinstancingManager::RegisterQueries()
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	{
		TypedElementQueryHandle QueryHandle = Database->RegisterQuery(
		Select(
		TEXT("Memento cleanup"),
		FProcessor(DSI::EQueryTickPhase::FrameEnd, Database->GetQueryTickGroupName(DSI::EQueryTickGroups::Default)),
			[this](TypedElementDataStorage::IQueryContext& Context, const TypedElementRowHandle* Row, const FTypedElementsReinstanceableSourceObject*)
			{
				Context.RemoveRows(Context.GetRowHandles());
				
				TConstArrayView<const FTypedElementsReinstanceableSourceObject> ReinstanceableArray = MakeArrayView(Context.GetColumn<FTypedElementsReinstanceableSourceObject>(), Context.GetRowCount());
				for (const FTypedElementsReinstanceableSourceObject& Reinstanceable : ReinstanceableArray)
				{
					OldObjectToMementoMap.Remove(Reinstanceable.Object);
				}
				
			})
			.Where().All<FTypedElementMementoTag, FTypedElementMementoPopulated>()
			.Compile()
		);
		check(QueryHandle != TypedElementInvalidQueryHandle);
	}
}

void UTypedElementObjectReinstancingManager::UnregisterQueries()
{
	// TODO: Observer queries cannot be unregistered in Mass at this time
}

void UTypedElementObjectReinstancingManager::HandleOnObjectPreRemoved(const void* Object, const FTypedElementDatabaseCompatibilityObjectTypeInfo& TypeInfo, TypedElementRowHandle ObjectRow)
{
	// This is the chance to record the old object to memento
	TypedElementRowHandle Memento = MementoSystem->CreateMemento(Database.Get());
	Database->AddOrGetColumn(ObjectRow, FTypedElementMementoOnDelete{ .Memento = Memento });
	
	Database->AddOrGetColumn(Memento, FTypedElementsReinstanceableSourceObject{.Object = Object});

	OldObjectToMementoMap.Add(Object, Memento);
}

void UTypedElementObjectReinstancingManager::HandleOnObjectsReinstanced(
	const FCoreUObjectDelegates::FReplacementObjectMap& ObjectReplacementMap)
{
	for (FCoreUObjectDelegates::FReplacementObjectMap::TConstIterator Iter = ObjectReplacementMap.CreateConstIterator(); Iter; ++Iter)
	{
		UObject* NewInstanceObject = Iter->Value;

		TypedElementRowHandle NewObjectRow = DataStorageCompatibility->FindRowWithCompatibleObjectExplicit(NewInstanceObject);
		if (!Database->IsRowAvailable(NewObjectRow))
		{
			NewObjectRow = DataStorageCompatibility->AddCompatibleObjectExplicit(NewInstanceObject);
		}
		
		const void* PreDeleteObject = Iter->Key;
		const TypedElementRowHandle* MementoRowPtr = OldObjectToMementoMap.Find(PreDeleteObject);
		if (MementoRowPtr != nullptr)
		{
			// Kick off re-instantiation of NewObjectRow from the Memento
			TypedElementRowHandle Memento = *MementoRowPtr;
			if (ensureMsgf(Database->HasColumns(Memento, TConstArrayView<const UScriptStruct*>({FTypedElementMementoTag::StaticStruct()})), TEXT("Cannot reinstantiate from a non memento row")))
			{
				Database->AddOrGetColumn(Memento, FTypedElementMementoReinstanceTarget{ .Target = NewObjectRow });
			}
		}
	}
}
