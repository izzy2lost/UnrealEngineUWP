// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Compatibility/TypedElementObjectReinstancingManager.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "Engine/World.h"
#include "Misc/Change.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"

#include "TypedElementDatabaseCompatibility.generated.h"

#if !defined(TEDS_SEPARATE_ACTOR_REGISTRATION)
#	define TEDS_SEPARATE_ACTOR_REGISTRATION 0
#endif

class AActor;
class ITypedElementDataStorageInterface;
struct FMassActorManager;

enum class ETypedElementDatabaseCompatibilityObjectType : uint8;
struct FTypedElementDatabaseCompatibilityObjectTypeInfo;

UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDatabaseCompatibility
	: public UObject
	, public ITypedElementDataStorageCompatibilityInterface
{
	GENERATED_BODY()
public:
	using ObjectAddedCallback = TFunction<void(const void* /*Object*/, const FTypedElementDatabaseCompatibilityObjectTypeInfo&, TypedElementRowHandle /*Row*/)>;
	using ObjectRemovedCallback = TFunction<void(const void* /*Object*/, const FTypedElementDatabaseCompatibilityObjectTypeInfo&, TypedElementRowHandle /*Row*/)>;
	
	~UTypedElementDatabaseCompatibility() override = default;

	void Initialize(ITypedElementDataStorageInterface* StorageInterface);
	void Deinitialize();

	void RegisterRegistrationFilter(ObjectRegistrationFilter Filter) override;
	void RegisterDealiaserCallback(ObjectToRowDealiaser Dealiaser) override;
	FDelegateHandle RegisterObjectAddedCallback(ObjectAddedCallback&& OnObjectAdded);
	void UnregisterObjectAddedCallback(FDelegateHandle Handle);
	FDelegateHandle RegisterObjectRemovedCallback(ObjectRemovedCallback&& OnObjectRemoved);
	void UnregisterObjectRemovedCallback(FDelegateHandle Handle);
	
	TypedElementRowHandle AddCompatibleObjectExplicit(UObject* Object) override;
	TypedElementRowHandle AddCompatibleObjectExplicit(UObject* Object, TypedElementTableHandle Table) override;
	TypedElementRowHandle AddCompatibleObjectExplicit(AActor* Actor) override;
	TypedElementRowHandle AddCompatibleObjectExplicit(AActor* Actor, TypedElementTableHandle Table) override;
	TypedElementRowHandle AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo);
	TypedElementRowHandle AddCompatibleObjectExplicit(void* Object, TWeakObjectPtr<const UScriptStruct> TypeInfo, TypedElementTableHandle Table);
	
	void RemoveCompatibleObjectExplicit(UObject* Object) override;
	void RemoveCompatibleObjectExplicit(AActor* Actor) override;
	void RemoveCompatibleObjectExplicit(void* Object) override;

	TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const UObject* Object) const override;
	TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const AActor* Actor) const override;
	TypedElementRowHandle FindRowWithCompatibleObjectExplicit(const void* Object) const override;

private:

	// The below changes expect UTypedElementDatabaseCompatibility to be the object passed in to StoreUndo.
	// Note that we cannot pass in TargetObject to StoreUndo because doing so seems to stomp regular Modify()
	// changes for that object.
	class FRegistrationCommandChange final : public FCommandChange
	{
	public:
		FRegistrationCommandChange(TypedElementDataStorage::TableHandle InTable,
			UObject* InTargetObject);

		void Apply(UObject* Object) override;
		void Revert(UObject* Object) override;
		FString ToString() const override;

	private:
		TypedElementDataStorage::TableHandle Table{ TypedElementDataStorage::InvalidTableHandle };
		TWeakObjectPtr<UObject> TargetObject;
	};
	class FDeregistrationCommandChange final : public FCommandChange
	{
	public:
		FDeregistrationCommandChange(TypedElementDataStorage::TableHandle InTable,
			UObject* InTargetObject);

		void Apply(UObject* Object) override;
		void Revert(UObject* Object) override;
		FString ToString() const override;

	private:
		TypedElementDataStorage::TableHandle Table{ TypedElementDataStorage::InvalidTableHandle };
		TWeakObjectPtr<UObject> TargetObject;
	};

	void Prepare();
	void Reset();
	void CreateStandardArchetypes();
	
	bool ShouldAddObject(const UObject* Object) const;
	template<bool bEnableTransactions>
	TypedElementRowHandle AddCompatibleObjectExplicitTransactionable(UObject* Object, TypedElementTableHandle Table);
	template<bool bEnableTransactions>
	void RemoveCompatibleObjectExplicitTransactionable(UObject* Object);
	TypedElementRowHandle DealiasObject(const UObject* Object) const;

	void Tick();
#if TEDS_SEPARATE_ACTOR_REGISTRATION
	void TickPendingActorRegistration(UWorld* EditorWorld);
#endif
	void TickPendingUObjectRegistration();
	void TickPendingExternalObjectRegistration();
	void TickObjectSync();

	void OnPostEditChangeProperty(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
	void OnObjectModified(UObject* Object);
	void OnObjectAdded(const void* Object, FTypedElementDatabaseCompatibilityObjectTypeInfo TypeInfo, TypedElementRowHandle Row) const;
	void OnPreObjectRemoved(const void* Object, FTypedElementDatabaseCompatibilityObjectTypeInfo TypeInfo, TypedElementRowHandle Row) const;
	
	void OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues);
	void OnPreWorldFinishDestroy(UWorld* World);

	void OnActorDestroyed(AActor* Actor);
	
	template<typename AddressType>
	struct PendingRegistration
	{
	private:
		TArray<AddressType> Addresses;
		TArray<TypedElementRowHandle> ReservedRowHandles;

	public:
		void Add(TypedElementRowHandle ReservedRowHandle, AddressType Address);
		bool IsEmpty() const;
		int32 Num() const;
		TArrayView<AddressType> GetAddresses();
		TArrayView<TypedElementRowHandle> GetReservedRowHandles();

		void RemoveInvalidEntries(ITypedElementDataStorageInterface& Storage, const TFunctionRef<bool(const AddressType&)>& Validator);
		void ProcessEntries(ITypedElementDataStorageInterface& Storage, TypedElementTableHandle Table,
			const TFunctionRef<void(TypedElementRowHandle, const AddressType&)>& SetupRowCallback);
		void Reset();
	};
	struct ExternalObjectRegistration
	{
		void* Object;
		TWeakObjectPtr<const UScriptStruct> TypeInfo;
	};
#if TEDS_SEPARATE_ACTOR_REGISTRATION
	TMap<TypedElementTableHandle, PendingRegistration<TWeakObjectPtr<AActor>>> ActorsPendingRegistration;
#endif
	TMap<TypedElementTableHandle, PendingRegistration<TWeakObjectPtr<UObject>>> UObjectsPendingRegistration;
	TMap<TypedElementTableHandle, PendingRegistration<ExternalObjectRegistration>> ExternalObjectsPendingRegistration;
	
	TArray<ObjectRegistrationFilter> ObjectRegistrationFilters;
	TArray<ObjectToRowDealiaser> ObjectToRowDialiasers;
	TArray<TPair<ObjectAddedCallback, FDelegateHandle>> ObjectAddedCallbackList;
	TArray<TPair<ObjectRemovedCallback, FDelegateHandle>> PreObjectRemovedCallbackList;

	TypedElementTableHandle StandardActorTable{ TypedElementInvalidTableHandle };
	TypedElementTableHandle StandardActorWithTransformTable{ TypedElementInvalidTableHandle };
	TypedElementTableHandle StandardUObjectTable{ TypedElementInvalidTableHandle };
	TypedElementTableHandle StandardExternalObjectTable{ TypedElementInvalidTableHandle };
	ITypedElementDataStorageInterface* Storage{ nullptr };
#if TEDS_SEPARATE_ACTOR_REGISTRATION
	TSharedPtr<FMassActorManager> ActorSubsystem;
#endif

	/**
	 * Reference of objects (UObject and AActor) that need to be fully synced from the world to the database.
	 * Caution: Could point to objects that have been GC-ed
	 */
	TSet<TObjectKey<const UObject>> ObjectsNeedingFullSync;

	TMap<UWorld*, FDelegateHandle> ActorDestroyedDelegateHandles;
	FDelegateHandle PostEditChangePropertyDelegateHandle;
	FDelegateHandle ObjectModifiedDelegateHandle;
	FDelegateHandle PostWorldInitializationDelegateHandle;
	FDelegateHandle PreWorldFinishDestroyDelegateHandle;
};

enum class ETypedElementDatabaseCompatibilityObjectType : uint8
{
	Struct,
	Class
};

/**
 * Objects with type info defined in either UScriptStruct or UClass can be stored into TEDS via the
 * ITypedElementDataStorageCompatibilityInterface
 * This is a discriminated union which aids with callbacks made when objects are added
 */
struct FTypedElementDatabaseCompatibilityObjectTypeInfo
{
	ETypedElementDatabaseCompatibilityObjectType TypeInfoType;

	union
	{
		const UScriptStruct* ScriptStruct;
		const UClass* Class;
	};

	FTypedElementDatabaseCompatibilityObjectTypeInfo(const UScriptStruct* InScriptStruct)
		: TypeInfoType(ETypedElementDatabaseCompatibilityObjectType::Struct)
		, ScriptStruct(InScriptStruct)
	{}

	FTypedElementDatabaseCompatibilityObjectTypeInfo(const UClass* InClass)
	: TypeInfoType(ETypedElementDatabaseCompatibilityObjectType::Class)
	, Class(InClass)
	{}

	FName GetFName() const;
};

inline FName FTypedElementDatabaseCompatibilityObjectTypeInfo::GetFName() const
{
	switch(TypeInfoType)
	{
	case ETypedElementDatabaseCompatibilityObjectType::Struct: return ScriptStruct->GetFName();
	case ETypedElementDatabaseCompatibilityObjectType::Class: return Class->GetFName();
	default: return FName();
	}
}