// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyBagRepository.h"
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
		InstanceDataObject->RemoveFromRoot();
		InstanceDataObject = nullptr;
	}
}

FPropertyBagRepository& FPropertyBagRepository::Get()
{
	static FPropertyBagRepository Repo;
	return Repo;
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

void FPropertyBagRepository::CreateInstanceDataObjectUnsafe(const UObjectBase* Owner, FPropertyBagAssociationData& BagData)
{
	check(!BagData.InstanceDataObject);	// No repeated calls
	const FPropertyBag* PropertyBag = BagData.Bag;
	// construct InstanceDataObject class
	// TODO: should we put the InstanceDataObject or it's class in a package?
	const UClass* InstanceDataObjectClass = CreateInstanceDataObjectClass(PropertyBag, Owner->GetClass(), GetTransientPackage());

	// construct InstanceDataObject object
	FStaticConstructObjectParameters Params(InstanceDataObjectClass);
	Params.SetFlags |= EObjectFlags::RF_Transactional;
	UObject* InstanceDataObjectObject = StaticConstructObject_Internal(Params);
	BagData.InstanceDataObject = InstanceDataObjectObject;
	
	// setup load context to mark properties the that were set by serialization
	FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
	TGuardValue<bool> ScopedImpersonateProperties(LoadContext->bImpersonateProperties, true);
	TGuardValue<bool> ScopedTrackSerializedPropertyPath(LoadContext->bTrackSerializedPropertyPath, true);
	
	const FDelegateHandle OnTaggedPropertySerializeHandle = LoadContext->OnTaggedPropertySerialize.AddLambda(
		[&BagData](const FUObjectSerializeContext& Context)
		{
			if (!Context.SerializedPropertyPath.IsEmpty())
			{
				MarkPropertySetBySerialization(BagData.InstanceDataObject, Context.SerializedPropertyPath);
			}
		}
	);

	UObject* OwnerAsObject = (UObject*)Owner;
	OwnerAsObject->SetFlags(RF_NeedLoad);
	OwnerAsObject->GetLinker()->Preload(OwnerAsObject);

	LoadContext->OnTaggedPropertySerialize.Remove(OnTaggedPropertySerializeHandle);
}

// Not sure this is necessary.
void FPropertyBagRepository::ShrinkMaps()
{
	FPropertyBagRepositoryLock LockRepo(this);
	AssociatedData.Compact();
}

} // UE
