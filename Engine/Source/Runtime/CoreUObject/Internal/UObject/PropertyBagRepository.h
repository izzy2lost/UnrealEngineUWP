// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"

class UObjectBase;
class UObject;

namespace UE
{

class FPropertyBag;

// Singleton class tracking property bag association with objects
class FPropertyBagRepository
{
	// TODO: Make private throughout and extend access permissions here or in wrapper classes? Don't want engine code modifying bags outside of serializers and details panels.
	//friend UObjectBase;
	//friend UStruct;

private:
	friend class FPropertyBagRepositoryLock;
	mutable FCriticalSection CriticalSection;
	
	// Lifetimes/ownership:
	// Managed within UObjectBase and synced with object lifetime. The repo tracks pointers to bags, not the bags themselves.
	// FPropertyBag destruction handles destruction of sub-bags automatically, via the FPropertyBagProperty tracking them.

	/** Map of objects/subobjects to their top level property bag. */
	// TODO: Currently will only exist in editor world, but could tracking per world make some sense for teardown in future? We're relying on object destruction to occur properly to free these up. 
	TMap<const UObjectBase*, FPropertyBag*> ObjectToPropertyBagMap;	// TODO: Ref bags via handle?
	// /** Map of subobject/container/struct/etc. paths (needs FPropertyBagPath) to their property bag. Might not want to track subobjects here (they'll be in ObjectToPropertyBagMap already). */
	// TMap<FSoftObjectPath, FPropertyBag*> ObjectPathToPropertySubBagMap;
	
	//TMap<const UObjectBase*, UObject*> ObjectToArchetypeMap;

	FPropertyBagRepository() = default;

public:
	FPropertyBagRepository(const FPropertyBagRepository &) = delete;
	FPropertyBagRepository& operator=(const FPropertyBagRepository&) = delete;
	
	// Singleton accessor
	static COREUOBJECT_API FPropertyBagRepository& Get();

	// Reclaim space - TODO: Hook up to GC.
	void ShrinkMaps();
	
	// TODO: Restrict bag creation to actor creation and UStruct::SerializeVersionedTaggedProperties?
	// Object owner is tracked internally
	FPropertyBag* CreateOuterBag(UObjectBase* Owner);

	// Future version for reworked archetypes - track archetype rather than bag (directly):
	/**
	 * Instantiate an archetype object representing all fields within the bag, tracked against the owner object.
	 * @param Owner			- Associated in world object.
	 * @return				- Custom archetype object, UClass derived from associated bag.
	 */
	//UObject* CreateArchetype(UObjectBase* Owner);

	// TODO: Restrict property bag  destruction to within UObject::BeginDestroy() & FPropertyBagProperty destructor.
	void DestroyOuterBag(UObjectBase* Owner);

	/**
	 * ReassociateObjects
	 * @param ReplacedObjects - old/new owner object pairs. Reassigns archetypes/bags to the new owner.
	 */
	COREUOBJECT_API void ReassociateObjects(const TMap<UObject*, UObject*>& ReplacedObjects);
	
	// Accessors
	FPropertyBag* FindBag(const UObjectBase* Owner);
	const FPropertyBag* FindBag(const UObjectBase* Owner) const;
	//UObject* FindArchetype(const UObjectBase* Owner);
	
	bool HasBag(const UObjectBase* Owner) const;
	
	// TODO: Cache floating property status - within bag?
	//EMatchStatus MatchesBag(const UObjectBase* Object) const;
	
private:
	void Lock() const { CriticalSection.Lock(); }
	void Unlock() const { CriticalSection.Unlock(); }

	// Not thread safe - internal use only.
	FPropertyBag* FindBagUnsafe(const UObjectBase* Object);
	const FPropertyBag* FindBagUnsafe(const UObjectBase* Object) const;
};

} // UE
