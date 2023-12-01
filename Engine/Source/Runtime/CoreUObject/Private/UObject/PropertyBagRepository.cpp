// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyBagRepository.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/PropertyBag.h"


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
	
	if(Archetype && Archetype->IsValidLowLevel())
	{
		Archetype->RemoveFromRoot();
		Archetype = nullptr;
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
		FPropertyBagAssociationData NewBagData(new FPropertyBag);
		NewBagData.Bag = new FPropertyBag;
		BagData = &AssociatedData.Emplace(Owner, NewBagData);
	}
	return BagData->Bag;
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

bool FPropertyBagRepository::HasArchetype(const UObjectBase* Object) const
{
	FPropertyBagRepositoryLock LockRepo(this);
	// May be lazily instantiated, but implied from existence of object data.
	return AssociatedData.Contains(Object);
}

UObject* FPropertyBagRepository::FindArchetype(const UObjectBase* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
	if(BagData && !BagData->Archetype)
	{
		CreateArchetypeUnsafe(Object, *BagData);
	}
	return BagData ? BagData->Archetype : nullptr;
}

const UObject* FPropertyBagRepository::FindArchetype(const UObjectBase* Object) const
{
	return const_cast<FPropertyBagRepository*>(this)->FindArchetype(Object);
}

void FPropertyBagRepository::CreateArchetypeUnsafe(const UObjectBase* Object, FPropertyBagAssociationData& BagData)
{
	check(BagData.Archetype);	// No repeated calls
	
	// TODO: Implementation
	// TODO: AddReferencedObject/AddToRoot GC handling.
	BagData.Archetype = nullptr;	// Guaranteed nullptr on failure.
}

// Not sure this is necessary.
void FPropertyBagRepository::ShrinkMaps()
{
	FPropertyBagRepositoryLock LockRepo(this);
	AssociatedData.Compact();
}

} // UE
