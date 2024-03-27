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

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPropertyBagRepository, Log, All);

namespace UE
{

class FPropertyBagPlaceholderTypeRegistry
{
public:
	void AddReferencedObjects(FReferenceCollector& Collector)
	{
		ConsumePendingPlaceholderTypes();
		Collector.AddReferencedObjects(PlaceholderTypes);
	}

	void Add(UStruct* Type)
	{
		PendingPlaceholderTypes.Enqueue(Type);
	}

	bool Contains(UStruct* Type)
	{
		ConsumePendingPlaceholderTypes();
		return PlaceholderTypes.Contains(Type);
	}

protected:
	void ConsumePendingPlaceholderTypes()
	{
		if (!PendingPlaceholderTypes.IsEmpty())
		{
			FScopeLock ScopeLock(&CriticalSection);

			TObjectPtr<UStruct> PendingType;
			while(PendingPlaceholderTypes.Dequeue(PendingType))
			{
				PlaceholderTypes.Add(PendingType);
			}
		}
	}

private:
	FCriticalSection CriticalSection;

	// List of types that have been registered.
	TSet<TObjectPtr<UStruct>> PlaceholderTypes;

	// Types that have been added but not yet registered. Utilizes a thread-safe queue so we can avoid race conditions during an async load.
	TQueue<TObjectPtr<UStruct>> PendingPlaceholderTypes;
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
	PropertyBagPlaceholderTypeRegistry = MakeUnique<FPropertyBagPlaceholderTypeRegistry>();
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectModified.AddLambda([](const UObject* Object)
	{
		// if this object is an InstanceDataObject, modify it's owner as well
		if (const UObject* Owner = Get().FindInstanceForDataObject(Object))
		{
			const_cast<UObject*>(Owner)->Modify();
		}
	});
#endif
}

void FPropertyBagRepository::ReassociateObjects(const TMap<UObject*, UObject*>& ReplacedObjects)
{
	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData OldBagData;
	for (const TPair<UObject*, UObject*>& Pair : ReplacedObjects)
	{
		if(AssociatedData.RemoveAndCopyValue(Pair.Key, OldBagData))
		{
			InstanceDataObjectToOwner.Remove(OldBagData.InstanceDataObject);
			if (Pair.Value != nullptr) // Pair.Value can be nullptr when an object was destroyed like for example a UClass when it's deleted
			{
				FPropertyBagAssociationData& NewBagData = AssociatedData.FindChecked(Pair.Value);
				
				InstanceDataObjectToOwner.Add(NewBagData.InstanceDataObject, Pair.Value);
				
				CopyPropertySetBySerializationData(
					OldBagData.InstanceDataObject->GetClass(), OldBagData.InstanceDataObject,
					NewBagData.InstanceDataObject->GetClass(), NewBagData.InstanceDataObject);
			}
			OldBagData.Destroy();
		}
		Namespaces.Remove(Pair.Key);
	}
}

void FPropertyBagRepository::CleanupLevel(const UObject* Level)
{
	FPropertyBagRepositoryLock LockRepo(this);
	TArray<UObject*> Instances = {const_cast<UObject*>(Level)};
	GetObjectsWithOuter(Level, Instances, true);
	for (const UObject* Instance : Instances)
	{
		RemoveAssociationUnsafe(Instance);
	}
}

// TODO: Create these by class on construction?
FPropertyBag* FPropertyBagRepository::CreateOuterBag(const UObject* Owner)
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

UObject* FPropertyBagRepository::CreateInstanceDataObject(UObject* Owner, FArchive* Archive)
{
	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData& BagData = AssociatedData.FindOrAdd(Owner);
	if(!BagData.InstanceDataObject)
	{
		CreateInstanceDataObjectUnsafe(Owner, BagData, Archive);
	}
	return BagData.InstanceDataObject;
}

// TODO: Remove this? Bag destruction to be handled entirely via UObject::BeginDestroy() (+ FPropertyBagProperty destructor)?
void FPropertyBagRepository::DestroyOuterBag(const UObject* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	RemoveAssociationUnsafe(Owner);
}

bool FPropertyBagRepository::RequiresFixup(const UObject* Object) const
{
	const FPropertyBag* PropertyBag = FindBag(Object);
	return !PropertyBag || PropertyBag->IsEmpty();
}

bool FPropertyBagRepository::RemoveAssociationUnsafe(const UObject* Owner)
{
	FPropertyBagAssociationData OldData;
	if(AssociatedData.RemoveAndCopyValue(Owner, OldData))
	{
		InstanceDataObjectToOwner.Remove(OldData.InstanceDataObject);
		OldData.Destroy();
		return true;
	}

	// note: RemoveAssociationUnsafe is called on every object regardless of whether it has a property bag.
	// in that scenario, there's a chance we have a namespace associated with it. Remove that namespace.
	Namespaces.Remove(Owner);
	return false;
}

bool FPropertyBagRepository::HasBag(const UObject* Object) const
{
	// TODO: Should be consistent across all objects of a given type, so handle via TStructOpsTypeTraits or similar?
	FPropertyBagRepositoryLock LockRepo(this);
	//return AssociatedData.Contains(Object);	// Better approach? Object data should guarantee existence of bag.
	return FindBag(Object) != nullptr;
}

FPropertyBag* FPropertyBagRepository::FindBag(const UObject* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
	return BagData ? BagData->Bag : nullptr;
}

const FPropertyBag* FPropertyBagRepository::FindBag(const UObject* Object) const
{
	return const_cast<FPropertyBagRepository*>(this)->FindBag(Object);
}

bool FPropertyBagRepository::HasInstanceDataObject(const UObject* Object) const
{
	FPropertyBagRepositoryLock LockRepo(this);
	// May be lazily instantiated, but implied from existence of object data.
	return AssociatedData.Contains(Object);
}

UObject* FPropertyBagRepository::FindInstanceDataObject(const UObject* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
	return BagData ? BagData->InstanceDataObject : nullptr;
}

const UObject* FPropertyBagRepository::FindInstanceDataObject(const UObject* Object) const
{
	return const_cast<FPropertyBagRepository*>(this)->FindInstanceDataObject(Object);
}

const UObject* FPropertyBagRepository::FindInstanceForDataObject(const UObject* InstanceDataObject) const
{
	FPropertyBagRepositoryLock LockRepo(this);
	const UObject* const* Owner = InstanceDataObjectToOwner.Find(InstanceDataObject);
	return Owner ? *Owner : nullptr;
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
	for (TPair<const UObject*, FPropertyBagAssociationData>& Element : AssociatedData)
	{
		Collector.AddReferencedObject(Element.Value.InstanceDataObject);
	}
	for (TPair<const UObject*, TObjectPtr<UObject>>& Element : Namespaces)
	{
		Collector.AddReferencedObject(Element.Value);
	}

	PropertyBagPlaceholderTypeRegistry->AddReferencedObjects(Collector);
}

FString FPropertyBagRepository::GetReferencerName() const
{
	return TEXT("FPropertyBagRepository");
}

void FPropertyBagRepository::CreateInstanceDataObjectUnsafe(UObject* Owner, FPropertyBagAssociationData& BagData, FArchive* Archive)
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

	// if an old IDO still exists with the same name, rename it out of the way so StaticConstructObject_Internal doesn't have conflicts
	if (UObject* OldIDO = StaticFindObjectFastInternal( /*Class=*/ nullptr, *OuterPtr, Owner->GetFName() ))
	{
		OldIDO->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders | REN_DoNotDirty);
	}

	// construct InstanceDataObject object
	FStaticConstructObjectParameters Params(InstanceDataObjectClass);
	Params.SetFlags |= EObjectFlags::RF_Transactional;
	Params.Name = Owner->GetFName();
	Params.Outer = *OuterPtr;
	UObject* InstanceDataObjectObject = StaticConstructObject_Internal(Params);
	BagData.InstanceDataObject = InstanceDataObjectObject;
	InstanceDataObjectToOwner.Add(InstanceDataObjectObject, Owner);
	
	// setup load context to mark properties the that were set by serialization
	FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
	TGuardValue<bool> ScopedImpersonateProperties(LoadContext->bImpersonateProperties, true);
	
	auto CopyTaggedProperties = [](const UObject* Source, UObject* Dest)
	{
		TArray<uint8> Buffer;
		Buffer.Reserve(Source->GetClass()->GetStructureSize());
		FObjectWriter Writer(Buffer);
		Source->GetClass()->SerializeTaggedProperties(Writer, (uint8*)Source, Source->GetClass(), (uint8*)Source->GetArchetype());
		
		FObjectReader Reader(Buffer);
		Reader.ArMergeOverrides = true;
		Dest->GetClass()->SerializeTaggedProperties(Reader, (uint8*)Dest, Dest->GetClass(), (uint8*)Dest->GetArchetype());
	};
	
	if (Archive)
	{
		// re-deserialize Owner but redirect it into the IDO instead using impersonation
		Owner->Serialize(*Archive);
	}
	else if (FLinkerLoad* Linker = Owner->GetLinker())
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

		// TODO: @jordan.hoffmann - this is very inefficient! We should remove this call to Preload. To do so, we'd need to change MarkPropertySetBySerialization
		// to cache the serialized property list in the property bag instead of the structs. We'd also need to copy the property bag values to the IDO
		Owner->SetFlags(RF_NeedLoad);
		Linker->Preload(Owner);
		LoadContext->OnTaggedPropertySerialize.Remove(OnTaggedPropertySerializeHandle);
		
		// copy data from owner to IDO
		CopyTaggedProperties(Owner, BagData.InstanceDataObject);
	}
	else
	{
		ensureMsgf(BagData.Bag == nullptr, TEXT("Linker missing when generating IDO for an object with loose properties. Loose properties will be lost"));
		// copy data from owner to IDO
		CopyTaggedProperties(Owner, BagData.InstanceDataObject);
	}
}

// Not sure this is necessary.
void FPropertyBagRepository::ShrinkMaps()
{
	FPropertyBagRepositoryLock LockRepo(this);
	AssociatedData.Compact();
	InstanceDataObjectToOwner.Compact();
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderType(UStruct* Type)
{
	if (!Type)
	{
		return false;
	}

	return FPropertyBagRepository::Get().PropertyBagPlaceholderTypeRegistry->Contains(Type);
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderObject(UObject* Object)
{
	if (!Object)
	{
		return false;
	}

	return Object->HasAnyFlags(RF_HasPlaceholderType|RF_ClassDefaultObject)
		&& IsPropertyBagPlaceholderType(Object->GetClass());
}

namespace Private
{
#if WITH_EDITOR
	static bool bEnablePropertyBagPlaceholderObjectSupport = 0;
	static FAutoConsoleVariableRef CVarEnablePropertyBagPlaceholderObjectSupport(
		TEXT("SceneGraph.EnablePropertyBagPlaceholderObjectSupport"),
		bEnablePropertyBagPlaceholderObjectSupport,
		TEXT("If true, allows placeholder types to be created in place of missing types on load in order to redirect serialization into a property bag."),
		ECVF_Default
	);
#endif
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderObjectSupportEnabled()
{
#if WITH_EDITOR && UE_WITH_OBJECT_HANDLE_TYPE_SAFETY
	static bool bIsInitialized = false;
	if (!bIsInitialized)
	{
		Private::bEnablePropertyBagPlaceholderObjectSupport = FParse::Param(FCommandLine::Get(), TEXT("WithPropertyBagPlaceholderObjects"));
		bIsInitialized = true;
	}
	
	return Private::bEnablePropertyBagPlaceholderObjectSupport;
#else
	return false;
#endif
}

bool FPropertyBagRepository::IsInstanceDataObjectSupportEnabled(UObject* InObject)
{
#if WITH_EDITOR
	return UE::IsInstanceDataObjectSupportEnabled(InObject);
#else
	return false;
#endif
}

UStruct* FPropertyBagRepository::CreatePropertyBagPlaceholderType(UObject* Outer, UClass* Class, FName Name, EObjectFlags Flags, UStruct* SuperStruct)
{
	UStruct* PlaceholderType = NewObject<UClass>(Outer, Class, Name, Flags);
	PlaceholderType->SetSuperStruct(SuperStruct);
	PlaceholderType->Bind();
	PlaceholderType->StaticLink(/*bRelinkExistingProperties =*/ true);

	// Extra configuration needed for class types.
	if (UClass* PlaceholderTypeAsClass = Cast<UClass>(PlaceholderType))
	{
		// Create and configure its CDO as if it were loaded - for non-native class types, this is required.
		UObject* PlaceholderClassDefaults = PlaceholderTypeAsClass->GetDefaultObject();
		PlaceholderTypeAsClass->PostLoadDefaultObject(PlaceholderClassDefaults);

		// This class is for internal use and should not be exposed for selection or instancing in the editor.
		PlaceholderTypeAsClass->ClassFlags |= CLASS_Hidden | CLASS_HideDropDown;
	}

	// Use the property bag repository for now to manage property bag placeholder types (e.g. object lifetime).
	// Note: The object lifetime of instances of this type will rely on existing references that are serialized.
	FPropertyBagRepository::Get().PropertyBagPlaceholderTypeRegistry->Add(PlaceholderType);

	return PlaceholderType;
}

} // UE
