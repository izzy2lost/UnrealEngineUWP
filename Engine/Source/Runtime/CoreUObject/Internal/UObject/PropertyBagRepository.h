// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "UObject/GCObject.h"

class UObject;

namespace UE
{

class FPropertyPathNameTree;

// Singleton class tracking property bag association with objects
class FPropertyBagRepository : public FGCObject
{
	struct FPropertyBagAssociationData
	{
		void Destroy();

		FPropertyPathNameTree* Tree = nullptr;
		TObjectPtr<UObject> InstanceDataObject = nullptr;
	};
	// TODO: Make private throughout and extend access permissions here or in wrapper classes? Don't want engine code modifying bags outside of serializers and details panels.
	//friend UObjectBase;
	//friend UStruct;
	friend struct FScopedInstanceDataObjectLoad;

private:
	friend class FPropertyBagRepositoryLock;
	mutable FCriticalSection CriticalSection;

	// Lifetimes/ownership:
	// Managed within UObjectBase and synced with object lifetime. The repo tracks pointers to bags, not the bags themselves.

	/** Map of objects/subobjects to their top level property bag. */
	// TODO: Currently will only exist in editor world, but could tracking per world make some sense for teardown in future? We're relying on object destruction to occur properly to free these up. 
	TMap<const UObject*, FPropertyBagAssociationData> AssociatedData;
	
	TMap<const UObject*, const UObject*> InstanceDataObjectToOwner;

	// used to make sure IDOs don't have name overlap
	TMap<const UObject*, TObjectPtr<UObject>> Namespaces;

	/** Internal registry that tracks the current set of types for property bag container objects instanced as placeholders for package exports that have invalid or missing class imports on load. */
	TUniquePtr<class FPropertyBagPlaceholderTypeRegistry> PropertyBagPlaceholderTypeRegistry;

	FPropertyBagRepository();

public:
	FPropertyBagRepository(const FPropertyBagRepository &) = delete;
	FPropertyBagRepository& operator=(const FPropertyBagRepository&) = delete;
	
	// Singleton accessor
	static COREUOBJECT_API FPropertyBagRepository& Get();

	// Reclaim space - TODO: Hook up to GC.
	void ShrinkMaps();

	/**
	 * Finds or creates a property path name tree to collect unknown property paths within the owner.
	 */
	FPropertyPathNameTree* CreateUnknownPropertyTree(const UObject* Owner);

	// Future version for reworked InstanceDataObjects - track InstanceDataObject rather than bag (directly):
	/**
	 * Instantiate an InstanceDataObject object representing all fields within the bag, tracked against the owner object.
	 * @param Owner			- Associated in world object.
	 * @param Archive		- used to read value of new archive from. Leave this set to nullptr to use the object's linker or copy Owner
	 * @return				- Custom InstanceDataObject object, UClass derived from associated bag.
	 */
	COREUOBJECT_API UObject* CreateInstanceDataObject(UObject* Owner, FArchive* Archive = nullptr);

	// TODO: Restrict property bag  destruction to within UObject::BeginDestroy() & FPropertyBagProperty destructor.
	// Removes bag, InstanceDataObject, and all associated data for this object.
	void DestroyOuterBag(const UObject* Owner);

	/**
	 * ReassociateObjects
	 * @param ReplacedObjects - old/new owner object pairs. Reassigns InstanceDataObjects/bags to the new owner.
	 */
	COREUOBJECT_API void ReassociateObjects(const TMap<UObject*, UObject*>& ReplacedObjects);

	/**
	 * CleanupLevel - Removes all IDOs for objects outered to the level
	 * @param Level - The level being cleaned up
	 */
	COREUOBJECT_API void CleanupLevel(const UObject* Level);

	static void PostEditChangeChainProperty(const UObject* Object, FPropertyChangedChainEvent& PropertyChangedEvent);

	/**
	 * RequiresFixup - test if InstanceDataObject properties perfectly match object instance properties. This is necessary for the object to be published in UEFN.    
	 * @param Object	- Object to test.
	 * @return			- Does the object's InstanceDataObject contain any loose properties requiring user fixup before the object may be published?
	 */
	COREUOBJECT_API bool RequiresFixup(const UObject* Object) const;

	// Accessors
	COREUOBJECT_API bool HasInstanceDataObject(const UObject* Owner) const;
	COREUOBJECT_API UObject* FindInstanceDataObject(const UObject* Owner);
	COREUOBJECT_API const UObject* FindInstanceDataObject(const UObject* Owner) const;

	COREUOBJECT_API const UObject* FindInstanceForDataObject(const UObject* InstanceDataObject) const;

	// query whether a property in Struct was set when the struct was deserialized
	COREUOBJECT_API static bool WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex = 0);

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	// End FGCObject interface

	// query for whether or not the given struct/class is a placeholder type
	static COREUOBJECT_API bool IsPropertyBagPlaceholderType(UStruct* Type);
	// query for whether or not the given object was created as a placeholder type
	static COREUOBJECT_API bool IsPropertyBagPlaceholderObject(UObject* Object);
	// query for whether or not creating property bag placeholder objects should be allowed
	static COREUOBJECT_API bool IsPropertyBagPlaceholderObjectSupportEnabled();
	// query whether an object supports IDO generation
	static COREUOBJECT_API bool IsInstanceDataObjectSupportEnabled(UObject* InObject = nullptr);

	/**
	 * Create a new placeholder type object to swap in for a missing class/struct. An object of
	 * this type will be associated with a property bag when serialized so it doesn't lose data.
	 * 
	 * @param Outer			Scope at which to create the placeholder type object (e.g. UPackage).
	 * @param Class			Type object class (or derivative type). For example, UClass::StaticClass().
	 * @param Name			Optional object name. If not specified, a unique object name will be created.
	 * @param Flags			Additional object flags. These will be appended to the default set of type object flags.
	 *						(Note: All placeholder types are transient by definition and internally default to 'RF_Transient'.)
	 * @param SuperStruct	Optional super type. By default, placeholder types are derivatives of UObject (NULL implies default).
	 * 
	 * @return A reference to a new placeholder type object.
	 */ 
	static COREUOBJECT_API UStruct* CreatePropertyBagPlaceholderType(UObject* Outer, UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags, UStruct* SuperStruct = nullptr);
	template<typename T = UObject>
	static UClass* CreatePropertyBagPlaceholderClass(UObject* Outer, UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags)
	{
		return Cast<UClass>(CreatePropertyBagPlaceholderType(Outer, Class, Name, Flags, T::StaticClass()));
	}

private:
	void Lock() const { CriticalSection.Lock(); }
	void Unlock() const { CriticalSection.Unlock(); }

	// Internal functions requiring the repository to be locked before being called

	// Delete owner reference and disassociate all data. Returns success.
	bool RemoveAssociationUnsafe(const UObject* Owner);

	// Instantiate InstanceDataObject within BagData. Returns InstanceDataObject object. 
	void CreateInstanceDataObjectUnsafe(UObject* Owner, FPropertyBagAssociationData& BagData, FArchive* Archive = nullptr);
};

} // UE
