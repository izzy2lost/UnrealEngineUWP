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

FPropertyBagRepository& FPropertyBagRepository::Get()
{
	static FPropertyBagRepository Repo;
	return Repo;
}

void FPropertyBagRepository::ReassociateObjects(const TMap<UObject*, UObject*>& ReplacedObjects)
{
	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBag* PropertyBag;
	for(const TPair<UObject*, UObject*>& Pair : ReplacedObjects)
	{
		if(ObjectToPropertyBagMap.RemoveAndCopyValue(Pair.Key, PropertyBag))
		{
			// We may see duplicate bags generated during TPS based duplication of the old object - these can be safely deleted/replaced, although ideally we shouldn't be creating new bags during duplication.
			if(FPropertyBag* DuplicateBag = ObjectToPropertyBagMap.FindRef(Pair.Value))
			{
				UE_LOG(LogPropertyBagRepository, Warning, TEXT("Duplicate property bag detected for %s"), *Pair.Value->GetName());
				delete DuplicateBag;
			}
			//UE_LOG(LogPropertyBagRepository, Log, TEXT("Bag fixup: %s (#%08x) -> %s (#%08x)"), *Pair.Key->GetName(), uint64(Pair.Key), *Pair.Value->GetName(), uint64(Pair.Value));
			ObjectToPropertyBagMap.Emplace(Pair.Value, PropertyBag);
		}
	}
}

// TODO: Create these by class on construction?
FPropertyBag* FPropertyBagRepository::CreateOuterBag(UObjectBase* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBag* PropertyBag = FindBagUnsafe(Owner);
	if(!PropertyBag)
	{
		PropertyBag = new FPropertyBag;
		ObjectToPropertyBagMap.Emplace(Owner, PropertyBag);
	}
	return PropertyBag;
}

// TODO: Remove this? Bag destruction to be handled entirely via UObject::BeginDestroy() (+ FPropertyBagProperty destructor)?
void FPropertyBagRepository::DestroyOuterBag(UObjectBase* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	UE::FPropertyBag* PropertyBag = nullptr;
	if(ObjectToPropertyBagMap.RemoveAndCopyValue(Owner, PropertyBag))
	{
		delete PropertyBag;
	}
}

FPropertyBag* FPropertyBagRepository::FindBag(const UObjectBase* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	return FindBagUnsafe(Object);
}

const FPropertyBag* FPropertyBagRepository::FindBag(const UObjectBase* Object) const
{
	FPropertyBagRepositoryLock LockRepo(this);
	return FindBagUnsafe(Object);
}

FPropertyBag* FPropertyBagRepository::FindBagUnsafe(const UObjectBase* Object)
{
	UE::FPropertyBag* PropertyBag = ObjectToPropertyBagMap.FindRef(Object);
	// Evaluate property bag support via type traits? During Assign?
	//UE_CLOG(!PropertyBag || Object->GetClass()->HasSerializeViaPropertyBag(), LogPropertyBagRepository, Warning, TEXT("Object %s PropertyBag is invalid: Doesn't serialize via property bags"), *static_cast<const UObjectBaseUtility*>(Object)->GetPathName());
	return PropertyBag;
}

const FPropertyBag* FPropertyBagRepository::FindBagUnsafe(const UObjectBase* Object) const
{
	return const_cast<FPropertyBagRepository*>(this)->FindBagUnsafe(Object);
}

bool FPropertyBagRepository::HasBag(const UObjectBase* Object) const
{
	// TODO: Should be consistent across all objects of a given type, so handle via TStructOpsTypeTraits or similar?
	return FindBag(Object) != nullptr;
}

// Not sure this is necessary.
void FPropertyBagRepository::ShrinkMaps()
{
	FPropertyBagRepositoryLock LockRepo(this);
	ObjectToPropertyBagMap.Compact();
}

} // UE
