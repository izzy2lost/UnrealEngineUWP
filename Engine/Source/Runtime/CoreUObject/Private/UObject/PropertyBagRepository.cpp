// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyBagRepository.h"
#include "Containers/Queue.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/PropertyBag.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/LinkerLoad.h"
#include "UObject/InstanceDataObjectUtils.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogPropertyBagRepository, Log, All);

namespace UE
{

class FPropertyBagTypeRegistry
{
public:
	void AddReferencedObjects(FReferenceCollector& Collector)
	{
		ConsumePendingPlaceholderTypes();
		Collector.AddReferencedObjects(PlaceholderTypes);
	}

	void Add(UClass* Class)
	{
		PendingPlaceholderTypes.Enqueue(Class);
	}

	bool Contains(UClass* Class)
	{
		ConsumePendingPlaceholderTypes();
		return PlaceholderTypes.Contains(Class);
	}

protected:
	void ConsumePendingPlaceholderTypes()
	{
		if (!PendingPlaceholderTypes.IsEmpty())
		{
			FScopeLock ScopeLock(&CriticalSection);

			TObjectPtr<UClass> Class;
			while(PendingPlaceholderTypes.Dequeue(Class))
			{
				PlaceholderTypes.Add(Class);
			}
		}
	}

private:
	FCriticalSection CriticalSection;

	// List of types that have been registered.
	TSet<TObjectPtr<UClass>> PlaceholderTypes;

	// Types that have been added but not yet registered. Utilizes a thread-safe queue so we can avoid race conditions during an async load.
	TQueue<TObjectPtr<UClass>> PendingPlaceholderTypes;
};

class FPropertyBagRepositoryLock
{
#if THREADSAFE_UOBJECTS
	const FPropertyBagRepository* Repo;	// Technically a singleton, but just in case...
#endif
public:
	FORCEINLINE FPropertyBagRepositoryLock(const FPropertyBagRepository* InRepo)
	{
#if THREADSAFE_UOBJECTS
		if (!(IsGarbageCollectingAndLockingUObjectHashTables() && IsInGameThread()))	// Mirror object hash tables behaviour exactly for now
		{
			Repo = InRepo;
			InRepo->Lock();
		}
		else
		{
			Repo = nullptr;
		}
#else
		check(IsInGameThread());
#endif
	}
	FORCEINLINE ~FPropertyBagRepositoryLock()
	{
#if THREADSAFE_UOBJECTS
		if (Repo)
		{
			Repo->Unlock();
		}
#endif
	}
};

void FPropertyBagRepository::FPropertyBagAssociationData::Destroy()
{
	delete Bag;
	Bag = nullptr;
	
	if(InstanceDataObject && InstanceDataObject->IsValidLowLevel())
	{
		InstanceDataObject = nullptr;
	}
}

FPropertyBagRepository& FPropertyBagRepository::Get()
{
	static FPropertyBagRepository Repo;
	return Repo;
}

FPropertyBagRepository::FPropertyBagRepository()
{
	PropertyBagTypeRegistry = MakeUnique<FPropertyBagTypeRegistry>();
}

void FPropertyBagRepository::ReassociateObjects(const TMap<UObject*, UObject*>& ReplacedObjects)
{
	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData BagData;
	for(const TPair<UObject*, UObject*>& Pair : ReplacedObjects)
	{
		if(AssociatedData.RemoveAndCopyValue(Pair.Key, BagData))
		{
			// We may see duplicate bags generated during TPS based duplication of the old object - these can be safely deleted/replaced, although ideally we shouldn't be creating new bags during duplication.
			if(RemoveAssociationUnsafe(Pair.Value))
			{
				UE_LOG(LogPropertyBagRepository, Warning, TEXT("Duplicate property bag detected for %s"), *Pair.Value->GetName());
			}
			//UE_LOG(LogPropertyBagRepository, Log, TEXT("Bag fixup: %s (#%08x) -> %s (#%08x)"), *Pair.Key->GetName(), uint64(Pair.Key), *Pair.Value->GetName(), uint64(Pair.Value));
			AssociatedData.Emplace(Pair.Value, BagData);
		}
	}
}

// TODO: Create these by class on construction?
FPropertyBag* FPropertyBagRepository::CreateOuterBag(const UObjectBase* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner);
	if(!BagData)
	{
		FPropertyBagAssociationData NewBagData;
		NewBagData.Bag = new FPropertyBag;
		BagData = &AssociatedData.Emplace(Owner, NewBagData);
	}
	return BagData->Bag;
}

UObject* FPropertyBagRepository::CreateInstanceDataObject(const UObjectBase* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData& BagData = AssociatedData.FindOrAdd(Owner);
	if(!BagData.InstanceDataObject)
	{
		CreateInstanceDataObjectUnsafe(Owner, BagData);
	}
	return BagData.InstanceDataObject;
}

// TODO: Remove this? Bag destruction to be handled entirely via UObject::BeginDestroy() (+ FPropertyBagProperty destructor)?
void FPropertyBagRepository::DestroyOuterBag(const UObjectBase* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	RemoveAssociationUnsafe(Owner);
}

bool FPropertyBagRepository::RequiresFixup(const UObjectBase* Object) const
{
	const FPropertyBag* PropertyBag = FindBag(Object);
	return !PropertyBag || PropertyBag->IsEmpty();
}

bool FPropertyBagRepository::RemoveAssociationUnsafe(const UObjectBase* Owner)
{
	FPropertyBagAssociationData OldData;
	if(AssociatedData.RemoveAndCopyValue(Owner, OldData))
	{
		OldData.Destroy();
		return true;
	}

	// note: RemoveAssociationUnsafe is called on every object regardless of whether it has a property bag.
	// in that scenario, there's a chance we have a namespace associated with it. Remove that namespace.
	Namespaces.Remove(Owner);
	return false;
}

bool FPropertyBagRepository::HasBag(const UObjectBase* Object) const
{
	// TODO: Should be consistent across all objects of a given type, so handle via TStructOpsTypeTraits or similar?
	FPropertyBagRepositoryLock LockRepo(this);
	//return AssociatedData.Contains(Object);	// Better approach? Object data should guarantee existence of bag.
	return FindBag(Object) != nullptr;
}

FPropertyBag* FPropertyBagRepository::FindBag(const UObjectBase* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
	return BagData ? BagData->Bag : nullptr;
}

const FPropertyBag* FPropertyBagRepository::FindBag(const UObjectBase* Object) const
{
	return const_cast<FPropertyBagRepository*>(this)->FindBag(Object);
}

bool FPropertyBagRepository::HasInstanceDataObject(const UObjectBase* Object) const
{
	FPropertyBagRepositoryLock LockRepo(this);
	// May be lazily instantiated, but implied from existence of object data.
	return AssociatedData.Contains(Object);
}

UObject* FPropertyBagRepository::FindInstanceDataObject(const UObjectBase* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
	return BagData ? BagData->InstanceDataObject : nullptr;
}

const UObject* FPropertyBagRepository::FindInstanceDataObject(const UObjectBase* Object) const
{
	return const_cast<FPropertyBagRepository*>(this)->FindInstanceDataObject(Object);
}

bool FPropertyBagRepository::WasPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path)
{
	return UE::WasPropertySetBySerialization(Object, Path);
}

bool FPropertyBagRepository::WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex)
{
	return UE::WasPropertySetBySerialization(Struct, StructData, Property, ArrayIndex);
}

void FPropertyBagRepository::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<const UObjectBase*, FPropertyBagAssociationData>& Element : AssociatedData)
	{
		Collector.AddReferencedObject(Element.Value.InstanceDataObject);
	}
	for (TPair<const UObjectBase*, TObjectPtr<UObject>>& Element : Namespaces)
	{
		Collector.AddReferencedObject(Element.Value);
	}

	PropertyBagTypeRegistry->AddReferencedObjects(Collector);
}

FString FPropertyBagRepository::GetReferencerName() const
{
	return TEXT("FPropertyBagRepository");
}

void FPropertyBagRepository::CreateInstanceDataObjectUnsafe(const UObjectBase* Owner, FPropertyBagAssociationData& BagData)
{
	check(!BagData.InstanceDataObject);	// No repeated calls
	const FPropertyBag* PropertyBag = BagData.Bag;
	// construct InstanceDataObject class
	// TODO: should we put the InstanceDataObject or it's class in a package?
	const UClass* InstanceDataObjectClass = CreateInstanceDataObjectClass(PropertyBag, Owner->GetClass(), GetTransientPackage());

	TObjectPtr<UObject>* OuterPtr;
	if (FPropertyBagAssociationData* OuterData = AssociatedData.Find(Owner->GetOuter()))
	{
		OuterPtr = &OuterData->InstanceDataObject;
	}
	else
	{
		OuterPtr = &Namespaces.FindOrAdd(Owner->GetOuter());
		if (*OuterPtr == nullptr)
		{
			*OuterPtr = CreatePackage(nullptr); // TODO: replace with dummy object
		}
	}

	// construct InstanceDataObject object
	FStaticConstructObjectParameters Params(InstanceDataObjectClass);
	Params.SetFlags |= EObjectFlags::RF_Transactional;
	Params.Name = Owner->GetFName();
	Params.Outer = *OuterPtr;
	UObject* InstanceDataObjectObject = StaticConstructObject_Internal(Params);
	BagData.InstanceDataObject = InstanceDataObjectObject;
	
	// setup load context to mark properties the that were set by serialization
	FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
	TGuardValue<bool> ScopedImpersonateProperties(LoadContext->bImpersonateProperties, true);
	TGuardValue<bool> ScopedTrackSerializedPropertyPath(LoadContext->bTrackSerializedPropertyPath, true);
	
	UObject* OwnerAsObject = (UObject*)Owner;
	if (FLinkerLoad* Linker = OwnerAsObject->GetLinker())
	{
		const FDelegateHandle OnTaggedPropertySerializeHandle = LoadContext->OnTaggedPropertySerialize.AddLambda(
			[&BagData](const FUObjectSerializeContext& Context)
			{
				if (!Context.SerializedPropertyPath.IsEmpty())
				{
					MarkPropertySetBySerialization(BagData.InstanceDataObject, Context.SerializedPropertyPath);
				}
			}
		);
		OwnerAsObject->SetFlags(RF_NeedLoad);
		Linker->Preload(OwnerAsObject);
		LoadContext->OnTaggedPropertySerialize.Remove(OnTaggedPropertySerializeHandle);
	}
	else if (ensureMsgf(BagData.Bag == nullptr, TEXT("Linker missing when generating IDO for an object with loose properties")))
	{
		TArray<uint8> Buffer;
		FObjectWriter(OwnerAsObject, Buffer);
		FObjectReader(BagData.InstanceDataObject, Buffer);
	}
}

// Not sure this is necessary.
void FPropertyBagRepository::ShrinkMaps()
{
	FPropertyBagRepositoryLock LockRepo(this);
	AssociatedData.Compact();
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderType(UClass* ClassType)
{
	if (!ClassType)
	{
		return false;
	}

	return FPropertyBagRepository::Get().PropertyBagTypeRegistry->Contains(ClassType);
}

void FPropertyBagRepository::AddPropertyBagPlaceholderType(UClass* ClassType)
{
	if (!ClassType)
	{
		return;
	}

	FPropertyBagRepository::Get().PropertyBagTypeRegistry->Add(ClassType);
}

} // UE
