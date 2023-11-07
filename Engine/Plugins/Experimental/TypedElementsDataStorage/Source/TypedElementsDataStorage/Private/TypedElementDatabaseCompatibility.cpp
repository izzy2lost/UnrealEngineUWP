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

#if TEDS_SEPARATE_ACTOR_REGISTRATION
#	include "MassActorEditorSubsystem.h"
#endif

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
	return AddCompatibleObjectExplicit(Object, StandardUObjectTable);
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(UObject* Object, TypedElementTableHandle Table)
{
	bool bCanAddObject = 
		ensureMsgf(Storage, TEXT("Trying to add a UObject to Typed Element's Data Storage before the storage is available.")) &&
		ShouldAddObject(Object);
	return bCanAddObject ? AddCompatibleObjectExplicitTransactionable<true>(Object, Table) : TypedElementDataStorage::InvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(AActor* Actor)
{
	return AddCompatibleObjectExplicit(Actor, StandardActorTable);
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(AActor* Actor, TypedElementTableHandle Table)
{
#if TEDS_SEPARATE_ACTOR_REGISTRATION
	if (ensureMsgf(Storage, TEXT("Trying to add an actor to Typed Element's Data Storage before the storage is available.")) &&
		ShouldAddObject(Actor))
	{
		// Registration is delayed for two reasons:
		//	1. Allows entity creation in a single batch rather than multiple individual additions.
		//	2. Provides an opportunity to filter out the actors that are created within MASS itself as those will already be registered.
		
		TypedElementRowHandle ReservedRow = Storage->ReserveRow();
		PendingRegistration<TWeakObjectPtr<AActor>>& Pending = ActorsPendingRegistration.FindOrAdd(Table);
		Pending.Add(ReservedRow, Actor);
		return ReservedRow;
	}
	return TypedElementInvalidRowHandle;
#else
	return AddCompatibleObjectExplicit(static_cast<UObject*>(Actor), Table);
#endif
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo)
{
	return AddCompatibleObjectExplicit(Object, MoveTemp(TypeInfo), StandardExternalObjectTable);
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicit(
	void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo, TypedElementTableHandle Table)
{
	using namespace TypedElementDataStorage;

	if (ensureMsgf(Storage, TEXT("Trying to add an object to Typed Element's Data Storage before the storage is available.")))
	{
		TypedElementRowHandle ReservedRow = Storage->ReserveRow();
		Storage->IndexRow(GenerateIndexHash(Object), ReservedRow);
		PendingRegistration<ExternalObjectRegistration>& Pending = ExternalObjectsPendingRegistration.FindOrAdd(Table);
		Pending.Add(ReservedRow, ExternalObjectRegistration{ .Object = Object, .TypeInfo = TypeInfo });
		return ReservedRow;
	}
	else
	{
		return TypedElementInvalidRowHandle;
	}
}

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(UObject* Object)
{
	using namespace TypedElementDataStorage;

#if TEDS_SEPARATE_ACTOR_REGISTRATION
	if (Object->IsA<AActor>())
	{
		RemoveCompatibleObjectExplicit(static_cast<AActor*>(Object));
	}
	else
#else
	{
		RemoveCompatibleObjectExplicitTransactionable<true>(Object);
	}
#endif
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

void UTypedElementDatabaseCompatibility::RemoveCompatibleObjectExplicit(AActor* Actor)
{
#if TEDS_SEPARATE_ACTOR_REGISTRATION
	checkf(Storage, TEXT("Removing compatible objects is not supported before Typed Element's Database compatibility manager has been initialized."));

	// If there is no actor subsystem it means that the world has been destroyed, including the MASS instance,
	// so there's no references to clean up.
	if (Storage && ActorSubsystem && Storage->IsAvailable())
	{
		FMassEntityHandle Entity = ActorSubsystem->GetEntityHandleFromActor(Actor);
		// If there's no entity it may:
		//	- have been deleted earlier, e.g. through an explicit delete.
		//	- be an actor that never had a world assigned and was therefore never registered.
		//	- have registered with a MASS instance in another world, e.g. one created for PIE.
		if (Entity.IsValid())
		{
			auto ActorStore = Storage->GetColumn<FMassActorFragment>(Entity.AsNumber());
			if (ActorStore && !ActorStore->IsOwnedByMass()) // Only remove actors that were externally created.
			{
				TypedElementRowHandle Row = Entity.AsNumber();

				const FTypedElementClassTypeInfoColumn* TypeInfoColumn = Storage->GetColumn<FTypedElementClassTypeInfoColumn>(Row);
				if (Storage->HasRowBeenAssigned(Row) && ensureMsgf(TypeInfoColumn, TEXT("Missing type information for removed actor at ptr 0x%p [%s]"), Actor, *Actor->GetName()))
				{
					OnPreObjectRemoved(Actor, TypeInfoColumn->TypeInfo.Get(), Row);
				}
				
				ActorSubsystem->RemoveHandleForActor(Actor);
				Storage->RemoveRow(Entity.AsNumber());
			}
		}
	}
#else
	RemoveCompatibleObjectExplicit(static_cast<UObject*>(Actor));
#endif
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const UObject* Object) const
{
	using namespace TypedElementDataStorage;

	if (Object && Storage && Storage->IsAvailable())
	{
#if TEDS_SEPARATE_ACTOR_REGISTRATION
		const AActor* Actor = Cast<AActor>(Object);
		if (Actor && ActorSubsystem)
		{
			FMassEntityHandle Entity = ActorSubsystem->GetEntityHandleFromActor(Actor);
			return Entity.IsValid() ? Entity.AsNumber() : DealiasObject(Object);
		}
		else
#endif
		{
			RowHandle Row = Storage->FindIndexedRow(GenerateIndexHash(Object));
			return Storage->IsRowAvailable(Row) ? Row : DealiasObject(Object);
		}


	}
	return TypedElementInvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const AActor* Actor) const
{
#if TEDS_SEPARATE_ACTOR_REGISTRATION
	if (Storage && ActorSubsystem && Storage->IsAvailable())
	{
		FMassEntityHandle Entity = ActorSubsystem->GetEntityHandleFromActor(Actor);
		return Entity.IsValid() ? Entity.AsNumber() : DealiasObject(Actor);
	}
	else
	{
		return TypedElementInvalidRowHandle;
	}
#else
	return FindRowWithCompatibleObjectExplicit(static_cast<const UObject*>(Actor));
#endif
}

TypedElementRowHandle UTypedElementDatabaseCompatibility::FindRowWithCompatibleObjectExplicit(const void* Object) const
{
	using namespace TypedElementDataStorage;

	return (Object && Storage && Storage->IsAvailable()) ? Storage->FindIndexedRow(GenerateIndexHash(Object)) : InvalidRowHandle;
}

void UTypedElementDatabaseCompatibility::Prepare()
{
#if TEDS_SEPARATE_ACTOR_REGISTRATION
	UMassActorEditorSubsystem* MassActorEditorSubsystem = Storage->GetExternalSystem<UMassActorEditorSubsystem>();
	check(MassActorEditorSubsystem);
	ActorSubsystem = MassActorEditorSubsystem->GetMutableActorManager().AsShared();
#endif

	CreateStandardArchetypes();
}

void UTypedElementDatabaseCompatibility::Reset()
{
#if TEDS_SEPARATE_ACTOR_REGISTRATION
	ActorSubsystem = nullptr;
#endif
}

void UTypedElementDatabaseCompatibility::CreateStandardArchetypes()
{
	StandardActorTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FMassActorFragment, FTypedElementUObjectColumn, FTypedElementClassTypeInfoColumn,
			FTypedElementObjectSourceTableColumn,
			FTypedElementLabelColumn, FTypedElementLabelHashColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardActorTable"));

	StandardActorWithTransformTable = Storage->RegisterTable(StandardActorTable,
		TTypedElementColumnTypeList<FTypedElementLocalTransformColumn>(),
		FName("Editor_StandardActorWithTransformTable"));

	StandardUObjectTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FTypedElementUObjectColumn, FTypedElementClassTypeInfoColumn,
			FTypedElementObjectSourceTableColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardUObjectTable"));

	StandardExternalObjectTable = Storage->RegisterTable(TTypedElementColumnTypeList<
			FTypedElementExternalObjectColumn, FTypedElementScriptStructTypeInfoColumn,
			FTypedElementObjectSourceTableColumn,
			FTypedElementSyncFromWorldTag>(), 
		FName("Editor_StandardExternalObjectTable"));
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

template<bool bEnableTransactions>
TypedElementRowHandle UTypedElementDatabaseCompatibility::AddCompatibleObjectExplicitTransactionable(UObject* Object, TypedElementTableHandle Table)
{
	using namespace TypedElementDataStorage;

#if TEDS_SEPARATE_ACTOR_REGISTRATION
	if (Object->IsA<AActor>())
	{
		return AddCompatibleObjectExplicit(static_cast<AActor*>(Object));
	}
	else
#endif
	{
		TypedElementRowHandle ReservedRow = Storage->ReserveRow();
		Storage->IndexRow(GenerateIndexHash(Object), ReservedRow);

		PendingRegistration<TWeakObjectPtr<UObject>>& Pending = UObjectsPendingRegistration.FindOrAdd(Table);
		Pending.Add(ReservedRow, Object);

		if constexpr (bEnableTransactions)
		{
			if (GUndo)
			{
				GUndo->StoreUndo(this, MakeUnique<FRegistrationCommandChange>(Table, Object));
			}
		}

		return ReservedRow;
	}
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
					FTypedElementObjectSourceTableColumn* Table = Storage->GetColumn<FTypedElementObjectSourceTableColumn>(Row);
					if (ensureMsgf(Table,
						TEXT("An object is removed from the Typed Element's Database compatibility manager that didn't contain a source table column.")))
					{
						// Reverting the deletion of a row relies on the original table used to create the row rather
						// than the mutated variant that may have been used at the time of destruction. The reasoning is
						// that using the original table will trigger the same update that the mutated versions will call
						// so both will result in the same mirrored data in the data storage after processing. The benefit
						// of taking the original data is that less information has to be stored in the memento table thus
						// reducing the memory requirements though at the cost of more table mutations during reconstruction.
						// This does however put more emphasis on storing any auxiliary data that can't be reconstructed from
						// the object, e.g. the selection column, through the memento system.
						GUndo->StoreUndo(this, MakeUnique<FDeregistrationCommandChange>(Table->SourceTable, Object));
					}
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
#if TEDS_SEPARATE_ACTOR_REGISTRATION
		TickPendingActorRegistration(EditorWorld);
#endif
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

#if TEDS_SEPARATE_ACTOR_REGISTRATION
void UTypedElementDatabaseCompatibility::TickPendingActorRegistration(UWorld* EditorWorld)
{
	if (!ActorsPendingRegistration.IsEmpty())
	{
		// Filter out the actors that are already registered or already destroyed. 
		// The most common case for this is actors created from within MASS.
		for (auto It = ActorsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.RemoveInvalidEntries(*Storage,
				[this, EditorWorld](const TWeakObjectPtr<AActor>& Actor)
				{
					AActor* Instance = Actor.Get();
					return Instance && Instance->GetWorld() == EditorWorld && !ActorSubsystem->GetEntityHandleFromActor(Instance).IsValid();
				});
		}

		// Add the remaining actors to the data storage.
		for (auto It = ActorsPendingRegistration.CreateIterator(); It; ++It)
		{
			It->Value.ProcessEntries(*Storage, It->Key,
				[this](TypedElementRowHandle Row, const TWeakObjectPtr<AActor>& ActorPtr)
				{
					FMassActorFragment* ActorStore = Storage->AddOrGetColumn<FMassActorFragment>(Row);
					checkf(ActorStore, TEXT("Failed to retrieve or add FMassActorFragment to newly created row."));

					constexpr bool bIsOwnedByMass = false;

					AActor* Actor = ActorPtr.Get();
					check(Actor);
					ActorStore->SetNoHandleMapUpdate(FMassEntityHandle::FromNumber(Row), Actor, bIsOwnedByMass);
					ActorSubsystem->SetHandleForActor(Actor, FMassEntityHandle::FromNumber(Row));

					Storage->AddOrGetColumn<FTypedElementUObjectColumn>(Row, FTypedElementUObjectColumn{ .Object = ActorPtr });
					Storage->AddOrGetColumn<FTypedElementClassTypeInfoColumn>(Row, FTypedElementClassTypeInfoColumn{ .TypeInfo = Actor->GetClass() });
					
					// Make sure the new row is tagged for update.
					Storage->AddColumn<FTypedElementSyncFromWorldTag>(Row);

					OnObjectAdded(ActorPtr.Get(), Actor->GetClass(), Row);
				});
		}
			
		ActorsPendingRegistration.Reset();
	}
}
#endif

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
#if !TEDS_SEPARATE_ACTOR_REGISTRATION
					if (AActor* Actor = Cast<AActor>(Object))
					{
						constexpr bool bIsOwnedByMass = false;
						Storage->AddOrGetColumn<FMassActorFragment>(Row)->SetNoHandleMapUpdate(FMassEntityHandle::FromNumber(Row), Actor, bIsOwnedByMass);
					}
#endif
					Storage->AddOrGetColumn<FTypedElementUObjectColumn>(Row, FTypedElementUObjectColumn{ .Object = Object });
					Storage->AddOrGetColumn<FTypedElementClassTypeInfoColumn>(Row, FTypedElementClassTypeInfoColumn{ .TypeInfo = Object->GetClass() });
					Storage->AddOrGetColumn<FTypedElementObjectSourceTableColumn>(Row, FTypedElementObjectSourceTableColumn{ .SourceTable = Table });
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
					Storage->AddOrGetColumn<FTypedElementObjectSourceTableColumn>(Row, FTypedElementObjectSourceTableColumn{ .SourceTable = Table });
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

UTypedElementDatabaseCompatibility::FRegistrationCommandChange::FRegistrationCommandChange(
	TypedElementDataStorage::TableHandle InTable, UObject* InTargetObject)
	: Table(InTable)
	, TargetObject(InTargetObject)
{
}

void UTypedElementDatabaseCompatibility::FRegistrationCommandChange::Apply(UObject* Object)
{
	if (UTypedElementDatabaseCompatibility* CompatibilityLayer = Cast<UTypedElementDatabaseCompatibility>(Object))
	{
		if (UObject* TargetRetrieved = TargetObject.Get(/*bEvenIfPendingKill=*/ true))
		{
			CompatibilityLayer->AddCompatibleObjectExplicitTransactionable<false>(TargetRetrieved, Table);
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

UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::FDeregistrationCommandChange(
	TypedElementDataStorage::TableHandle InTable, UObject* InTargetObject)
	: Table(InTable)
	, TargetObject(InTargetObject)
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
			CompatibilityLayer->AddCompatibleObjectExplicitTransactionable<false>(TargetRetrieved, Table);
		}
	}
}

FString UTypedElementDatabaseCompatibility::FDeregistrationCommandChange::ToString() const
{
	return TEXT("Typed Element Data Storage Compatibility - Deregistration");
}
