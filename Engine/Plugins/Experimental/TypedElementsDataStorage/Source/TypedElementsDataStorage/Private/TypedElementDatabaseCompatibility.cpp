// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseCompatibility.h"

#include "Algo/Unique.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementIndexHasher.h"
#include "MassActorSubsystem.h"
#include "TypedElementDataStorageProfilingMacros.h"

void UTypedElementDatabaseCompatibility::Initialize(ITypedElementDataStorageInterface* StorageInterface)
{
	checkf(StorageInterface, TEXT("Typed Element's Database compatibility manager is being initialized with an invalid storage target."));
	
	Storage = StorageInterface;
	Prepare();

	StorageInterface->OnUpdate().AddUObject(this, &UTypedElementDatabaseCompatibility::Tick);

	PostEditChangePropertyDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UTypedElementDatabaseCompatibility::OnPostEditChangeProperty);
	ObjectModifiedDelegateHandle = FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &UTypedElementDatabaseCompatibility::OnObjectModified);
	
	PostWorldInitializationDelegateHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UTypedElementDatabaseCompatibility::OnPostWorldInitialization);
	PreWorldFinishDestroyDelegateHandle = FWorldDelegates::OnPreWorldFinishDestroy.AddUObject(this, &UTypedElementDatabaseCompatibility::OnPreWorldFinishDestroy);
}

void UTypedElementDatabaseCompatibility::Deinitialize()
{
	for (TPair<UWorld*, FDelegateHandle>& It : ActorDestroyedDelegateHandles)
	{
		It.Key->RemoveOnActorDestroyededHandler(It.Value);
	}

	FWorldDelegates::OnPreWorldFinishDestroy.Remove(PreWorldFinishDestroyDelegateHandle);
	FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationDelegateHandle);

	FCoreUObjectDelegates::OnObjectModified.Remove(ObjectModifiedDelegateHandle);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PostEditChangePropertyDelegateHandle);
	
	Reset();
}

void UTypedElementDatabaseCompatibility::RegisterRegistrationFilter(ObjectRegistrationFilter Filter)
{
	ObjectRegistrationFilters.Add(MoveTemp(Filter));
}

void UTypedElementDatabaseCompatibility::RegisterDealiaserCallback(ObjectToRowDealiaser Dealiaser)
{
	ObjectToRowDialiasers.Add(MoveTemp(Dealiaser));
}

void UTypedElementDatabaseCompatibility::RegisterTypeTableAssociation(
	TObjectPtr<UStruct> TypeInfo, TypedElementDataStorage::TableHandle Table)
{
	TypeToTableMap.Add(TypeInfo, Table);
}

FDelegateHandle UTypedElementDatabaseCompatibility::RegisterObjectAddedCallback(ObjectAddedCallback&& OnObjectAdded)
{
	FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	ObjectAddedCallbackList.Emplace(MoveTemp(OnObjectAdded), Handle);
	return Handle;
}

void UTypedElementDatabaseCompatibility::UnregisterObjectAddedCallback(FDelegateHandle Handle)
{
	ObjectAddedCallbackList.RemoveAll([Handle](const TPair<ObjectAddedCallback, FDelegateHandle>& Element)->bool
	{
		return Element.Value == Handle;
	});
}

FDelegateHandle UTypedElementDatabaseCompatibility::RegisterObjectRemovedCallback(ObjectRemovedCallback&& OnObjectAdded)
{
	FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	PreObjectRemovedCallbackList.Emplace(MoveTemp(OnObjectAdded), Handle);
	return Handle;
}

void UTypedElementDatabaseCompatibility::UnregisterObjectRemovedCallback(FDelegateHandle Handle)
{
	PreObjectRemovedCallbackList.RemoveAll([Handle](const TPair<ObjectRemovedCallback, FDelegateHandle>& Element)->bool
	{
		return Element.Value == Handle;
	});
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(UObject* Object)
{
	bool bCanAddObject =
		ensureMsgf(Storage, TEXT("Trying to add a UObject to Typed Element's Data Storage before the storage is available.")) &&
		ShouldAddObject(Object);
	return bCanAddObject ? AddCompatibleObjectExplicitTransactionable<true>(Object) : TypedElementDataStorage::InvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo)
{
	using namespace TypedElementDataStorage;
	
	if (ensureMsgf(Storage, TEXT("Trying to add an object to Typed Element's Data Storage before the storage is available.")))
	{
		TypedElementRowHandle Result = FindRowWithCompatibleObjectExplicit(Object);
		if (!Storage->IsRowAvailable(Result))
		{
			TableHandle Table = FindBestMatchingTable(TypeInfo.Get());
			Table = (Table != InvalidTableHandle) ? Table : StandardExternalObjectTable;

			Result = Storage->ReserveRow();
			Storage->IndexRow(GenerateIndexHash(Object), Result);
			PendingRegistration<ExternalObjectRegistration>& Pending = ExternalObjectsPendingRegistration.FindOrAdd(Table);
			Pending.Add(Result, ExternalObjectRegistration{ .Object = Object, .TypeInfo = TypeInfo });
		}
		return Result;
	}
	else
	{
		return TypedElementInvalidRowHandle;
	}
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(UObject* Object)
{
	RemoveCompatibleObjectExplicitTransactionable<true>(Object);
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(void* Object)
{
	using namespace TypedElementDataStorage;

	checkf(Storage, TEXT("Removing compatible objects is not supported before Typed Element's Database compatibility manager has been initialized."));
	IndexHash Hash = GenerateIndexHash(Object);
	RowHandle Row = Storage->FindIndexedRow(Hash);
	if (Storage->IsRowAvailable(Row))
	{
		const FTypedElementScriptStructTypeInfoColumn* TypeInfoColumn = Storage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(Row);
		if (Storage->HasRowBeenAssigned(Row) && ensureMsgf(TypeInfoColumn, TEXT("Missing type information for removed void* object at ptr 0x%p"), Object))
		{
			OnPreObjectRemoved(Object, TypeInfoColumn->TypeInfo.Get(), Row);
		}
		Storage->RemoveRow(Row);
	}
}


TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const UObject* Object) const
{
	using namespace TypedElementDataStorage;

	if (Object && Storage && Storage->IsAvailable())
	{
		RowHandle Row = Storage->FindIndexedRow(GenerateIndexHash(Object));
		return Storage->IsRowAvailable(Row) ? Row : DealiasObject(Object);
	}
	return TypedElementInvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const void* Object) const
{
	using namespace TypedElementDataStorage;

	return (Object && Storage && Storage->IsAvailable()) ? Storage->FindIndexedRow(GenerateIndexHash(Object)) : InvalidRowHandle;
}

void UTypedElementDatabaseCompatibility::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UTypedElementDatabaseCompatibility* Compatibility = Cast<UTypedElementDatabaseCompatibility>(InThis);
	checkf(Compatibility, TEXT("AddReferencedObjects was given a null pointer or an object wasn't a typed element data compatibility object."));

	Collector.AddReferencedObjects(Compatibility->TypeToTableMap);
}

void UTypedElementDatabaseCompatibility::Prepare()
{
	CreateStandardArchetypes();
}

void UTypedElementDatabaseCompatibility::Reset()
{
}

void UTypedElementDatabaseCompatibility::CreateStandardArchetypes()
{
	StandardActorTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FMassActorFragment, FTypedElementUObjectColumn, FTypedElementClassTypeInfoColumn,
			FTypedElementLabelColumn, FTypedElementLabelHashColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardActorTable"));

	StandardActorWithTransformTable = Storage->RegisterTable(StandardActorTable,
		TTypedElementColumnTypeList<FTypedElementLocalTransformColumn>(),
		FName("Editor_StandardActorWithTransformTable"));

	StandardUObjectTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FTypedElementUObjectColumn, FTypedElementClassTypeInfoColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardUObjectTable"));

	StandardExternalObjectTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FTypedElementExternalObjectColumn, FTypedElementScriptStructTypeInfoColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardExternalObjectTable"));

	RegisterTypeTableAssociation(AActor::StaticClass(), StandardActorTable);
	RegisterTypeTableAssociation(UObject::StaticClass(), StandardUObjectTable);
}

bool UTypedElementDatabaseCompatibility::ShouldAddObject(const UObject* Object) const
{
	using namespace TypedElementDataStorage;

	bool Include = true;
	if (!Storage->IsRowAvailable(Storage->FindIndexedRow(GenerateIndexHash(Object))))
	{
		const ObjectRegistrationFilter* Filter = ObjectRegistrationFilters.GetData();
		const ObjectRegistrationFilter* FilterEnd = Filter + ObjectRegistrationFilters.Num();
		for (; Include && Filter != FilterEnd; ++Filter)
		{
			Include = (*Filter)(*this, Object);
		}
	}
	return Include;
}

TypedElementDataStorage::TableHandle UTypedElementDatabaseCompatibility::FindBestMatchingTable(const UStruct* TypeInfo) const
{
	using namespace TypedElementDataStorage;

	while (TypeInfo)
	{
		if (const TableHandle* Table = TypeToTableMap.Find(TypeInfo))
		{
			return *Table;
		}
		TypeInfo = TypeInfo->GetSuperStruct();
	}

	return InvalidTableHandle;
}

template<bool bEnableTransactions>
TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicitTransactionable(UObject* Object)
{
	using namespace TypedElementDataStorage;

	TypedElementRowHandle Result = FindRowWithCompatibleObjectExplicit(Object);
	if (!Storage->IsRowAvailable(Result))
	{
		TableHandle Table = FindBestMatchingTable(Object->GetClass());
		checkf(Table != InvalidTableHandle, TEXT("The Typed Elements Data Storage could not find any matching tables for object of type '%s'. "
			"This can mean that the object doesn't derive from UObject or that a table for UObject is no longer registered."), *Object->GetClass()->GetFName().ToString());

		Result = Storage->ReserveRow();
		Storage->IndexRow(GenerateIndexHash(Object), Result);

		PendingRegistration<TWeakObjectPtr<UObject>>& Pending = UObjectsPendingRegistration.FindOrAdd(Table);
		Pending.Add(Result, Object);

		if constexpr (bEnableTransactions)
		{
			if (GUndo)
			{
				GUndo->StoreUndo(this, MakeUnique<FRegistrationCommandChange>(Object));
			}
		}
	}

	return Result;
}

template<bool bEnableTransactions>
void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicitTransactionable(UObject* Object)
{
	using namespace TypedElementDataStorage;

	checkf(Storage, 
		TEXT("Removing compatible objects is not supported before Typed Element's Database compatibility manager has been initialized."));
	IndexHash Hash = GenerateIndexHash(Object);
	RowHandle Row = Storage->FindIndexedRow(Hash);
	if (Storage->IsRowAvailable(Row))
	{
		const FTypedElementClassTypeInfoColumn* TypeInfoColumn = Storage->GetColumn<FTypedElementClassTypeInfoColumn>(Row);
		if (Storage->HasRowBeenAssigned(Row) && 
			ensureMsgf(TypeInfoColumn, TEXT("Missing type information for removed UObject at ptr 0x%p [%s]"), Object, *Object->GetName()))
		{
			OnPreObjectRemoved(Object, TypeInfoColumn->TypeInfo.Get(), Row);

			if constexpr (bEnableTransactions)
			{
				if (GUndo)
				{
					GUndo->StoreUndo(this, MakeUnique<FDeregistrationCommandChange>(Object));
				}
			}
		}

		Storage->RemoveRow(Row);
	}
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::DealiasObject(const UObject* Object) const
{
	for (const ObjectToRowDealiaser& Dealiaser : ObjectToRowDialiasers)
	{
		if (TypedElementRowHandle Row = Dealiaser(*this, Object); Storage->IsRowAvailable(Row))
		{
			return Row;
		}
	}
	return TypedElementDataStorage::InvalidRowHandle;
}

void UTypedElementDatabaseCompatibility::Tick()
{
	TEDS_EVENT_SCOPE(TEXT("Compatibility Tick"))
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	// Delay processing until the required systems are available by not clearing any lists or doing any work.
	if (Storage && Storage->IsAvailable() && EditorWorld)
	{
		TickPendingUObjectRegistration();
		TickPendingExternalObjectRegistration();
		TickObjectSync();
	}
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Add(TypedElementRowHandle ReservedRowHandle, AddressType Address)
{
	Addresses.Add(Forward<AddressType>(Address));
	ReservedRowHandles.Add(ReservedRowHandle);
}

template<typename AddressType>
bool UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::IsEmpty() const
{
	// ReservedRowHandles can also be returned as they'll both have the same length.
	return Addresses.IsEmpty();
}

template<typename AddressType>
int32 UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Num() const
{
	// ReservedRowHandles can also be returned as they'll both have the same length.
	return Addresses.Num();
}

template<typename AddressType>
TArrayView<AddressType> UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::GetAddresses()
{
	return Addresses;
}

template<typename AddressType>
TArrayView<TypedElementRowHandle> UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::GetReservedRowHandles()
{
	return ReservedRowHandles;
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::RemoveInvalidEntries(
	ITypedElementDataStorageInterface& StorageInterface, const TFunctionRef<bool(const AddressType&)>& Validator)
{
	checkf(Addresses.Num() == ReservedRowHandles.Num(),
		TEXT("The reserved row handle count (%i) didn't match the stored pointer count (%i)."),
		ReservedRowHandles.Num(), Addresses.Num());

	AddressType* AddressBegin = Addresses.GetData();
	AddressType* AddressIt = Addresses.GetData();
	AddressType* AddressEnd = AddressBegin + Addresses.Num();
	TypedElementRowHandle* RowHandleBegin = ReservedRowHandles.GetData();
	TypedElementRowHandle* RowHandleIt = ReservedRowHandles.GetData();
	while (AddressIt != AddressEnd)
	{
		if (StorageInterface.IsRowAvailable(*RowHandleIt) && Validator(*AddressIt))
		{
			++AddressIt;
			++RowHandleIt;
		}
		else
		{
			// Don't shrink the registration array as the array will be reused with a variety of different object counts.
			// If memory size becomes an issue it's better to resize the array once after this loop rather than within the
			// loop to avoid many resizes happening.
			constexpr bool bAllowToShrink = false;
			StorageInterface.RemoveRow(*RowHandleIt);
			Addresses.RemoveAtSwap(AddressIt - AddressBegin, 1, bAllowToShrink);
			ReservedRowHandles.RemoveAtSwap(RowHandleIt - RowHandleBegin, 1, bAllowToShrink);
			--AddressEnd;
		}
	}
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::ProcessEntries(ITypedElementDataStorageInterface& StorageInterface,
	TypedElementTableHandle Table, const TFunctionRef<void(TypedElementRowHandle, const AddressType&)>& SetupRowCallback)
{
	if (!IsEmpty())
	{
		AddressType* TargetIt = GetAddresses().GetData();
		AddressType* TargetEnd = TargetIt + Num();
		StorageInterface.BatchAddRow(Table, GetReservedRowHandles(), 
			[this, &SetupRowCallback, &TargetIt, TargetEnd](TypedElementRowHandle Row)
			{
				SetupRowCallback(Row, *TargetIt);
				checkf(TargetIt < TargetEnd, TEXT("More (%i) entities were added than were requested (%i)."), TargetEnd - TargetIt, Num());
				++TargetIt;
			});
	}
}

template<typename AddressType>
void UTypedElementDatabaseCompatibility::PendingRegistration<AddressType>::Reset()
{
	Addresses.Reset();
	ReservedRowHandles.Reset();
}

void UTypedElementDatabaseCompatibility::TickPendingUObjectRegistration()
{
	if (!UObjectsPendingRegistration.IsEmpty())
	{
		// Filter out the objects that are already registered or already destroyed. 
		for (auto It = UObjectsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.RemoveInvalidEntries(*Storage,
				[](const TWeakObjectPtr<UObject>& Object)
				{
					return Object.Get() != nullptr;
				});
		}

		// Add the remaining object to the data storage.
		for (auto It = UObjectsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.ProcessEntries(*Storage, It->Key,
				[Table = It->Key, this](TypedElementRowHandle Row, const TWeakObjectPtr<UObject>& Object)
				{
					if (AActor* Actor = Cast<AActor>(Object))
					{
						constexpr bool bIsOwnedByMass = false;
						Storage->AddOrGetColumn<FMassActorFragment>(Row)->SetNoHandleMapUpdate(FMassEntityHandle::FromNumber(Row), Actor, bIsOwnedByMass);
					}

					Storage->AddOrGetColumn<FTypedElementUObjectColumn>(Row, FTypedElementUObjectColumn{ .Object = Object });
					Storage->AddOrGetColumn<FTypedElementClassTypeInfoColumn>(Row, FTypedElementClassTypeInfoColumn{ .TypeInfo = Object->GetClass() });
					// Make sure the new row is tagged for update.
					Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);
					OnObjectAdded(Object.Get(), Object->GetClass(), Row);
				});
		}

		UObjectsPendingRegistration.Reset();
	}
}

void UTypedElementDatabaseCompatibility::TickPendingExternalObjectRegistration()
{
	if (!ExternalObjectsPendingRegistration.IsEmpty())
	{
		// Filter out the objects that are already registered or already destroyed. 
		for (auto It = ExternalObjectsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.RemoveInvalidEntries(*Storage,
				[](const ExternalObjectRegistration& Object)
				{
					return Object.Object != nullptr;
				});
		}

		// Add the remaining object to the data storage.
		for (auto It = ExternalObjectsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.ProcessEntries(*Storage, It->Key, 
				[Table = It->Key, this](TypedElementRowHandle Row, const ExternalObjectRegistration& Object)
				{
					Storage->AddOrGetColumn<FTypedElementExternalObjectColumn>(Row, FTypedElementExternalObjectColumn{ .Object = Object.Object });
					Storage->AddOrGetColumn<FTypedElementScriptStructTypeInfoColumn>(Row, FTypedElementScriptStructTypeInfoColumn{ .TypeInfo = Object.TypeInfo });
					// Make sure the new row is tagged for update.
					Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);

					OnObjectAdded(Object.Object, Object.TypeInfo.Get(), Row);
				});
		}

		ExternalObjectsPendingRegistration.Reset();
	}
}

void UTypedElementDatabaseCompatibility::TickObjectSync()
{
	if (!ObjectsNeedingFullSync.IsEmpty())
	{
		TEDS_EVENT_SCOPE(TEXT("Process ObjectsNeedingFullSync"));
		
		TArray<TypedElementRowHandle> RowHandles;
		{
			TEDS_EVENT_SCOPE(TEXT("Reverse lookup Rows from Objects"));

			RowHandles.SetNumUninitialized(ObjectsNeedingFullSync.Num());
			{
				int32 RowHandleIndex = 0;
				for (TObjectKey<const UObject> ObjectKey : ObjectsNeedingFullSync)
				{
					const TypedElementRowHandle Row = FindRowWithCompatibleObject(ObjectKey);
					if (Row != TypedElementInvalidRowHandle)
					{
						RowHandles[RowHandleIndex++] = Row;
					}
				}
				const int32 RowHandleCount = RowHandleIndex;
				const bool bAllowShrinking = false;
				RowHandles.SetNum(RowHandleCount, bAllowShrinking);
			}

			ObjectsNeedingFullSync.Reset();
		}

		{
			TEDS_EVENT_SCOPE(TEXT("Add SyncFromWorld Tag"));
			// Tag the rows containing object data that so they get synced
			// Note: Watch out for the performance of this, may end up doing a lot of row moves
			for (TypedElementRowHandle Row : RowHandles)
			{
				Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);
			}
		}
	}
}

void UTypedElementDatabaseCompatibility::OnPostEditChangeProperty(
	UObject* Object,
	FPropertyChangedEvent& /*PropertyChangedEvent*/)
{
	// Determining the object is being tracked in the database can't be done safely as it may be queued for addition.
	// It would also add a small bit of performance overhead as access the lookup table can be done faster as a
	// batch operation during the tick step.
	ObjectsNeedingFullSync.FindOrAdd(Object);
}

void UTypedElementDatabaseCompatibility::OnObjectModified(UObject* Object)
{
	// Determining the object is being tracked in the database can't be done safely as it may be queued for addition.
	// It would also add a small bit of performance overhead as access the lookup table can be done faster as a
	// batch operation during the tick step.
	ObjectsNeedingFullSync.FindOrAdd(Object);
}

void UTypedElementDatabaseCompatibility::OnObjectAdded(const void* Object, FTypedElementDatabaseCompatibilityObjectTypeInfo TypeInfo, TypedElementRowHandle Row) const
{
	for (const TPair<ObjectAddedCallback, FDelegateHandle>& CallbackPair : ObjectAddedCallbackList)
	{
		const ObjectAddedCallback& Callback = CallbackPair.Key;
		Callback(Object, TypeInfo, Row);
	}
}

void UTypedElementDatabaseCompatibility::OnPreObjectRemoved(const void* Object, FTypedElementDatabaseCompatibilityObjectTypeInfo TypeInfo, TypedElementRowHandle Row) const
{
	for (const TPair<ObjectRemovedCallback, FDelegateHandle>& CallbackPair : PreObjectRemovedCallbackList)
	{
		const ObjectRemovedCallback& Callback = CallbackPair.Key;
		Callback(Object, TypeInfo, Row);
	}
}

void UTypedElementDatabaseCompatibility::OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues)
{
	FDelegateHandle Handle = World->AddOnActorDestroyedHandler(
		FOnActorDestroyed::FDelegate::CreateUObject(this, &UTypedElementDatabaseCompatibility::OnActorDestroyed));
	ActorDestroyedDelegateHandles.Add(World, Handle);
}

void UTypedElementDatabaseCompatibility::OnPreWorldFinishDestroy(UWorld* World)
{
	FDelegateHandle Handle;
	if (ActorDestroyedDelegateHandles.RemoveAndCopyValue(World, Handle))
	{
		World->RemoveOnActorDestroyededHandler(Handle);
	}
}

void UTypedElementDatabaseCompatibility::OnActorDestroyed(AActor* Actor)
{
	RemoveCompatibleObjectExplicit(Actor);
}



//
// UTypedElementDatabaseCompatibility::FRegistrationCommandChange
//

UTypedElementDatabaseCompatibility::FRegistrationCommandChange::FRegistrationCommandChange(UObject* InTargetObject)
	: TargetObject(InTargetObject)
{
}

void UTypedElementDatabaseCompatibility::FRegistrationCommandChange::Apply(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->AddCompatibleObjectExplicitTransactionable<false>(TargetRetrieved);
		}
	}
}

void UTypedElementDatabaseCompatibility::FRegistrationCommandChange::Revert(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->RemoveCompatibleObjectExplicitTransactionable<false>(TargetRetrieved);
		}
	}
}

FString UTypedElementDatabaseCompatibility::FRegistrationCommandChange::ToString() const
{
	return TEXT("Typed Element Data Storage Compatibility - Registration");
}


//
// UTypedElementDatabaseCompatibility::FDeregistrationCommandChange
//

UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::FDeregistrationCommandChange(UObject* InTargetObject)
	: TargetObject(InTargetObject)
{
}

void UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::Apply(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->RemoveCompatibleObjectExplicitTransactionable<false>(TargetRetrieved);
		}
	}
}

void UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::Revert(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->AddCompatibleObjectExplicitTransactionable<false>(TargetRetrieved);
		}
	}
}

FString UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::ToString() const
{
	return TEXT("Typed Element Data Storage Compatibility - Deregistration");
}
