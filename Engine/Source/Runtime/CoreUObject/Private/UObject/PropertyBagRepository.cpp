// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyBagRepository.h"

#if WITH_EDITORONLY_DATA

#include "Containers/Queue.h"
#include "Misc/StringBuilder.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "String/ParseTokens.h"
#include "UObject/GarbageCollection.h"
#include "UObject/InstanceDataObjectUtils.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/PropertyOptional.h"
#include "UObject/PropertyPathNameTree.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectThreadContext.h"
#include "Templates/UnrealTemplate.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPropertyBagRepository, Log, All);

namespace UE
{

/** Defined in InstanceDataObjectUtils.cpp */
void CopyTaggedProperties(const UObject* Source, UObject* Dest);

/** Internal registry that tracks the current set of types for property bag container objects instanced as placeholders for package exports that have invalid or missing class imports on load. */
class FPropertyBagPlaceholderTypeRegistry
{
public:
	static FPropertyBagPlaceholderTypeRegistry& Get()
	{
		static FPropertyBagPlaceholderTypeRegistry Instance;
		return Instance;
	}

	void AddReferencedObjects(FReferenceCollector& Collector)
	{
		ConsumePendingPlaceholderTypes();
		Collector.AddReferencedObjects(PlaceholderTypes);
	}

	void Add(UStruct* Type)
	{
		PendingPlaceholderTypes.Enqueue(Type);
	}

	void Remove(UStruct* Type)
	{
		PlaceholderTypes.Remove(Type);
	}

	bool Contains(const UStruct* Type)
	{
		ConsumePendingPlaceholderTypes();
		return PlaceholderTypes.Contains(Type);
	}

protected:
	FPropertyBagPlaceholderTypeRegistry() = default;
	FPropertyBagPlaceholderTypeRegistry(const FPropertyBagPlaceholderTypeRegistry&) = delete;
	FPropertyBagPlaceholderTypeRegistry& operator=(const FPropertyBagPlaceholderTypeRegistry&) = delete;

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
	delete Tree;
	Tree = nullptr;

	delete EnumNames;
	EnumNames = nullptr;

	if (InstanceDataObject && InstanceDataObject->IsValidLowLevel())
	{
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
	if (!IsInstanceDataObjectSupportEnabled())
	{
		return;
	}

	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData OldBagData;
	for (const TPair<UObject*, UObject*>& Pair : ReplacedObjects)
	{
		if (AssociatedData.RemoveAndCopyValue(Pair.Key, OldBagData))
		{
			InstanceDataObjectToOwner.Remove(OldBagData.InstanceDataObject);
			if (Pair.Value != nullptr) // Pair.Value can be nullptr when an object was destroyed like for example a UClass when it's deleted
			{
				FPropertyBagAssociationData& NewBagData = AssociatedData.FindChecked(Pair.Value);
				
				InstanceDataObjectToOwner.Add(NewBagData.InstanceDataObject, Pair.Value);
				
				CopyPropertyValueSerializedData(
					OldBagData.InstanceDataObject->GetClass(), OldBagData.InstanceDataObject,
					NewBagData.InstanceDataObject->GetClass(), NewBagData.InstanceDataObject);
			}
			OldBagData.Destroy();
		}
		else if (UStruct* TypeObject = Cast<UStruct>(Pair.Key))
		{
			if (IsPropertyBagPlaceholderType(TypeObject))
			{
				FPropertyBagPlaceholderTypeRegistry::Get().Remove(TypeObject);
			}
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

static FProperty* FindPropertyByNameAndType(const UStruct* Struct, FName InName, FName Type)
{
	for (FProperty* Property = Struct->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
	{
		if (Property->GetFName() == InName && Property->GetID() == Type)
		{
			return Property;
		}
	}

	return nullptr;
}

static bool ConstructRemappedPropertyChain(const FEditPropertyChain& Chain, FEditPropertyChain& NewChain, const UObject* Destination)
{
	UStruct* Struct = Destination->GetClass();
	for (FEditPropertyChain::TDoubleLinkedListNode* Itr = Chain.GetHead(); Itr; Itr = Itr->GetNextNode())
	{
		FProperty* Property = Itr->GetValue();
		Property = FindPropertyByNameAndType(Struct, Property->GetFName(), Property->GetID());
		if (!Property)
		{
			NewChain.Empty();
			return false;
		}
		NewChain.AddTail(Property);

		// iterate the struct to look in
		if (FOptionalProperty* AsOptionalProperty = CastField<FOptionalProperty>(Property))
		{
			Property = AsOptionalProperty->GetValueProperty();
		}
		else if (FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
		{
			Property = AsArrayProperty->Inner;
		}
		else if (FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
		{
			Property = AsSetProperty->ElementProp;
		}
		else if (FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
		{
			Property = AsMapProperty->ValueProp;
		}
		
		if (FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
		{
			Struct = AsStructProperty->Struct;
		}
		else
		{
			check(Itr->GetNextNode() == nullptr);
		}

		// remap active and active member nodes
		if (Chain.GetActiveNode() == Itr)
		{
			NewChain.SetActivePropertyNode(NewChain.GetTail()->GetValue());
		}
		if (Chain.GetActiveMemberNode() == Itr)
		{
			NewChain.SetActiveMemberPropertyNode(NewChain.GetTail()->GetValue());
		}
	}
	return true;
}

static void* ResolveChangePath(const void* StructData, FPropertyChangedChainEvent& ChangeEvent, bool bGrowContainersWhenNeeded = false)
{
	if (ChangeEvent.PropertyChain.GetHead() == nullptr)
	{
		return nullptr;
	}
		
	FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = ChangeEvent.PropertyChain.GetHead();
	void* MemoryPtr = const_cast<void*>(StructData);
	do
	{
		const FProperty* Property = PropertyNode->GetValue();
		MemoryPtr = Property->ContainerPtrToValuePtr<uint8>(MemoryPtr);
		PropertyNode = PropertyNode->GetNextNode();
		
		const int32 ArrayIndex = ChangeEvent.GetArrayIndex(Property->GetName());
		if (PropertyNode && ArrayIndex != INDEX_NONE)
		{
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property->GetOwnerProperty()))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, MemoryPtr);
				if(!ArrayHelper.IsValidIndex(ArrayIndex))
				{
					return nullptr;
				}
				MemoryPtr = ArrayHelper.GetRawPtr(ArrayIndex);

				// skip to the next property node already
				PropertyNode = PropertyNode->GetNextNode();
			}
			if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property->GetOwnerProperty()))
			{
				FScriptSetHelper SetHelper(SetProperty, MemoryPtr);
				if(!SetHelper.IsValidIndex(ArrayIndex))
				{
					return nullptr;
				}
				MemoryPtr = SetHelper.GetElementPtr(ArrayIndex);

				// skip to the next property node already
				PropertyNode = PropertyNode->GetNextNode();
			}
			if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property->GetOwnerProperty()))
			{
				FScriptMapHelper MapHelper(MapProperty, MemoryPtr);
				if(!MapHelper.IsValidIndex(ArrayIndex))
				{
					return nullptr;
				}
				MemoryPtr = MapHelper.GetValuePtr(ArrayIndex);

				// skip to the next property node already
				PropertyNode = PropertyNode->GetNextNode();
			}
		}
	}
	while (PropertyNode);

	return MemoryPtr;
}
static void CopyProperty(const FProperty* SourceProperty, const void* SourceValue, const FProperty* DestProperty, void* DestValue)
{
	check(SourceProperty->GetID() == DestProperty->GetID());
	if (SourceProperty->SameType(DestProperty))
	{
		SourceProperty->CopySingleValue(DestValue, SourceValue);
	}
	else if (const FStructProperty* SourcePropertyAsStruct = CastField<FStructProperty>(SourceProperty))
	{
		const UStruct* SourceStruct = SourcePropertyAsStruct->Struct;
		UStruct* DestStruct = CastFieldChecked<FStructProperty>(DestProperty)->Struct;
		for (FProperty* SourceChild : TFieldRange<FProperty>(SourceStruct))
		{
			if (FProperty* DestChild = FindPropertyByNameAndType(DestStruct, SourceChild->GetFName(), SourceChild->GetID()))
			{
				CopyProperty(SourceChild, SourceChild->ContainerPtrToValuePtr<void>(SourceValue),
					DestChild, DestChild->ContainerPtrToValuePtr<void>(DestValue));
			}
		}
	}
	else if (const FOptionalProperty* SourcePropertyAsOptional = CastField<FOptionalProperty>(SourceProperty))
	{
		const FOptionalProperty* DestPropertyAsOptional = CastFieldChecked<FOptionalProperty>(DestProperty);
		FOptionalPropertyLayout SourceOptionalLayout(SourcePropertyAsOptional->GetValueProperty());
		FOptionalPropertyLayout DestOptionalLayout(DestPropertyAsOptional->GetValueProperty());
		if (!SourceOptionalLayout.IsSet(SourceValue))
		{
			DestOptionalLayout.MarkUnset(DestValue);
		}
		else
		{
			const void* SourceChildValue = SourceOptionalLayout.GetValuePointerForRead(SourceValue);
			void* DestChildValue = DestOptionalLayout.MarkSetAndGetInitializedValuePointerToReplace(DestValue);
			
			CopyProperty(SourceOptionalLayout.GetValueProperty(), SourceChildValue,
				DestOptionalLayout.GetValueProperty(), DestChildValue);
		}
	}
	else if (const FArrayProperty* SourcePropertyAsArray = CastField<FArrayProperty>(SourceProperty))
	{
		const FArrayProperty* DestPropertyAsArray = CastFieldChecked<FArrayProperty>(DestProperty);
		FScriptArrayHelper SourceArray(SourcePropertyAsArray, SourceValue);
		FScriptArrayHelper DestArray(DestPropertyAsArray, DestValue);
		DestArray.Resize(SourceArray.Num());
		for (int32 I = 0; I < SourceArray.Num(); ++I)
		{
			CopyProperty(SourcePropertyAsArray->Inner, SourceArray.GetElementPtr(I),
					DestPropertyAsArray->Inner, DestArray.GetElementPtr(I));
		}
	}
	else if (const FSetProperty* SourcePropertyAsSet = CastField<FSetProperty>(SourceProperty))
	{
		const FSetProperty* DestPropertyAsSet = CastFieldChecked<FSetProperty>(DestProperty);
		FScriptSetHelper SourceSet(SourcePropertyAsSet, SourceValue);
		FScriptSetHelper DestSet(DestPropertyAsSet, DestValue);
		DestSet.Set->Empty(0, DestSet.SetLayout);
		for (FScriptSetHelper::FIterator Itr = SourceSet.CreateIterator(); Itr; ++Itr)
		{
			void* DestChild = DestSet.GetElementPtr(DestSet.AddUninitializedValue());
			DestSet.ElementProp->InitializeValue(DestChild);
			
			CopyProperty(SourceSet.ElementProp, SourceSet.GetElementPtr(Itr.GetInternalIndex()),
					DestSet.ElementProp, DestChild);
		}
		DestSet.Rehash();
	}
	else if (const FMapProperty* SourcePropertyAsMap = CastField<FMapProperty>(SourceProperty))
	{
		const FMapProperty* DestPropertyAsMap = CastFieldChecked<FMapProperty>(DestProperty);
		FScriptMapHelper SourceMap(SourcePropertyAsMap, SourceValue);
		FScriptMapHelper DestMap(DestPropertyAsMap, DestValue);
		DestMap.EmptyValues();
		for (FScriptMapHelper::FIterator Itr = SourceMap.CreateIterator(); Itr; ++Itr)
		{
			void* DestChildKey = DestMap.GetKeyPtr(DestMap.AddUninitializedValue());
			DestMap.KeyProp->InitializeValue(DestChildKey);
			
			CopyProperty(SourceMap.KeyProp, SourceMap.GetKeyPtr(Itr.GetInternalIndex()),
					DestMap.KeyProp, DestChildKey);
			
			void* DestChildValue = DestMap.GetValuePtr(DestMap.AddUninitializedValue());
			DestMap.ValueProp->InitializeValue(DestChildValue);
			
			CopyProperty(SourceMap.ValueProp, SourceMap.GetValuePtr(Itr.GetInternalIndex()),
					DestMap.ValueProp, DestChildValue);
		}
		DestMap.Rehash();
	}
}

static void AddProperty(const FProperty* SourceProperty, const void* SourceValue, const FProperty* DestProperty, void* DestValue, int32 ArrayIndex)
{
	if (const FArrayProperty* SourcePropertyAsArray = CastField<FArrayProperty>(SourceProperty))
	{
		const FArrayProperty* DestPropertyAsArray = CastFieldChecked<FArrayProperty>(DestProperty);
		FScriptArrayHelper SourceArray(SourcePropertyAsArray, SourceValue);
        FScriptArrayHelper DestArray(DestPropertyAsArray, DestValue);
		if (DestArray.Num() < ArrayIndex + 1)
		{
			DestArray.Resize(ArrayIndex + 1);
		}
		CopyProperty(SourcePropertyAsArray->Inner, SourceArray.GetElementPtr(ArrayIndex),
			DestPropertyAsArray->Inner, DestArray.GetElementPtr(ArrayIndex));
	}
	else if (const FSetProperty* SourcePropertyAsSet = CastField<FSetProperty>(SourceProperty))
	{
		const FSetProperty* DestPropertyAsSet = CastFieldChecked<FSetProperty>(DestProperty);
		FScriptSetHelper SourceSet(SourcePropertyAsSet, SourceValue);
        FScriptSetHelper DestSet(DestPropertyAsSet, DestValue);
		int32 DestArrayIndex = DestSet.AddUninitializedValue();
		
		void* DestElementPtr = DestSet.GetElementPtr(DestArrayIndex);
		DestPropertyAsSet->ElementProp->InitializeValue(DestElementPtr);
		CopyProperty(SourcePropertyAsSet->ElementProp, SourceSet.GetElementPtr(ArrayIndex),
			DestPropertyAsSet->ElementProp, DestElementPtr);
		DestSet.Rehash();
	}
	else if (const FMapProperty* SourcePropertyAsMap = CastField<FMapProperty>(SourceProperty))
	{
		const FMapProperty* DestPropertyAsMap = CastFieldChecked<FMapProperty>(DestProperty);
		FScriptMapHelper SourceMap(SourcePropertyAsMap, SourceValue);
		FScriptMapHelper DestMap(DestPropertyAsMap, DestValue);
		int32 DestArrayIndex = DestMap.AddUninitializedValue();

		void* DestKeyPtr = DestMap.GetKeyPtr(DestArrayIndex);
		DestPropertyAsMap->KeyProp->InitializeValue(DestKeyPtr);
		CopyProperty(SourcePropertyAsMap->KeyProp, SourceMap.GetKeyPtr(ArrayIndex),
			DestPropertyAsMap->KeyProp, DestKeyPtr);
		
		void* DestValuePtr = DestMap.GetValuePtr(DestArrayIndex);
		DestPropertyAsMap->ValueProp->InitializeValue(DestValuePtr);
		CopyProperty(SourcePropertyAsMap->ValueProp, SourceMap.GetValuePtr(ArrayIndex),
			DestPropertyAsMap->ValueProp, DestValuePtr);
		DestMap.Rehash();
	}
}

void FPropertyBagRepository::PostEditChangeChainProperty(const UObject* Object, FPropertyChangedChainEvent& PropertyChangedEvent)
{
#if WITH_EDITOR
	static TSet<TSoftObjectPtr<UObject>> ChangeCallbacksToSkip;
	if (ChangeCallbacksToSkip.Remove(Object))
	{
		// avoids infinite recursion
		return;
	}
	
	auto CopyChanges = [&PropertyChangedEvent](const UObject* Source, UObject* Dest) 
	{
		FEditPropertyChain RemappedChain;
		if (ConstructRemappedPropertyChain(PropertyChangedEvent.PropertyChain, RemappedChain, Dest))
		{
			Dest->PreEditChange(RemappedChain);
            
            FPropertyChangedChainEvent RemappedChangeEvent(RemappedChain, PropertyChangedEvent);
            const void* SourceData = ResolveChangePath(Source, PropertyChangedEvent);
            void* DestData = ResolveChangePath(Dest, RemappedChangeEvent, true);
            FProperty* SourceProperty = PropertyChangedEvent.PropertyChain.GetTail()->GetValue();
            FProperty* DestProperty = RemappedChangeEvent.PropertyChain.GetTail()->GetValue();
			
            if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
            {
            	int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(SourceProperty->GetName());
            	check(ArrayIndex != INDEX_NONE);
            	AddProperty(SourceProperty, SourceData, DestProperty, DestData, ArrayIndex);
            }
            else
            {
            	CopyProperty(SourceProperty, SourceData, DestProperty, DestData);
            }
            
            Dest->PostEditChangeChainProperty(RemappedChangeEvent);
		}
		else
		{
			ensureMsgf(false, TEXT("A const loose property was modified on an instance data object"));
		}
		
	};
	
	if (UObject* Ido = Get().FindInstanceDataObject(Object))
	{
		// if this object is an instance, modify it's IDO as well
		ChangeCallbacksToSkip.Add(Ido); // avoid infinite recursion
		CopyChanges(Object, Ido);
	}
	else if (UObject* Instance = const_cast<UObject*>(Get().FindInstanceForDataObject(Object)))
	{
		// if this object is an InstanceDataObject, modify it's owner as well
		ChangeCallbacksToSkip.Add(Instance); // avoid infinite recursion
		CopyChanges(Object, Instance);
	}
#endif
}

// TODO: Create these by class on construction?
FPropertyPathNameTree* FPropertyBagRepository::FindOrCreateUnknownPropertyTree(const UObject* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner);
	if (!BagData)
	{
		BagData = &AssociatedData.Emplace(Owner);
	}
	if (!BagData->Tree)
	{
		BagData->Tree = new FPropertyPathNameTree;
	}
	return BagData->Tree;
}

void FPropertyBagRepository::AddUnknownEnumName(const UObject* Owner, const UEnum* Enum, FPropertyTypeName EnumTypeName, FName EnumValueName)
{
	check(Owner);
	checkf(Enum || !EnumTypeName.IsEmpty(), TEXT("AddUnknownEnumName requires an enum or its type name. Owner: %s"), *Owner->GetPathName());

	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData& BagData = AssociatedData.FindOrAdd(Owner);
	if (!BagData.EnumNames)
	{
		BagData.EnumNames = new FUnknownEnumNames;
	}

	BagData.EnumNames->Add(Enum, EnumTypeName, EnumValueName);
}

void FUnknownEnumNames::Add(const UEnum* Enum, FPropertyTypeName EnumTypeName, FName EnumValueName)
{
	check(Enum || !EnumTypeName.IsEmpty());

	if (EnumTypeName.IsEmpty())
	{
		FPropertyTypeNameBuilder Builder;
		Builder.AddPath(Enum);
		EnumTypeName = Builder.Build();
	}

	FUnknownEnumNames::FInfo& Info = Enums.FindOrAdd(EnumTypeName);

	TStringBuilder<128> EnumValueString(InPlace, EnumValueName);
	if (String::FindFirstChar(EnumValueString, TEXT('|')) == INDEX_NONE)
	{
		if (int32 ColonIndex = String::FindFirst(EnumValueString, TEXTVIEW("::")); ColonIndex != INDEX_NONE)
		{
			Info.Names.Emplace(EnumValueString.ToView().RightChop(ColonIndex + TEXTVIEW("::").Len()));
		}
		else
		{
			Info.Names.Add(EnumValueName);
		}
	}
	else
	{
		Info.bHasFlags = true;
		String::ParseTokens(EnumValueString, TEXT('|'), [&Info, Enum](FStringView Token)
		{
			FName Name(Token);
			if (!Enum || Enum->GetIndexByName(Name) == INDEX_NONE)
			{
				Info.Names.Add(Name);
			}
		}, String::EParseTokensOptions::SkipEmpty | String::EParseTokensOptions::Trim);
	}

	if (!Info.bHasFlags && Enum && Enum->HasAnyEnumFlags(EEnumFlags::Flags))
	{
		Info.bHasFlags = true;
	}
}

void FPropertyBagRepository::FindUnknownEnumNames(const UObject* Owner, FPropertyTypeName EnumTypeName, TArray<FName>& OutNames, bool& bOutHasFlags) const
{
	checkf(!EnumTypeName.IsEmpty(), TEXT("FindUnknownEnumNames requires an enum type name. Owner: %s"), *Owner->GetPathName());

	OutNames.Empty();
	bOutHasFlags = false;

	if (const FUnknownEnumNames* EnumNames = FindUnknownEnumNames(Owner))
	{
		EnumNames->Find(EnumTypeName, OutNames, bOutHasFlags);
	}
}

const FUnknownEnumNames* FPropertyBagRepository::FindUnknownEnumNames(const UObject* Owner) const
{
	check(Owner);

	FPropertyBagRepositoryLock LockRepo(this);
	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner);
	return BagData ? BagData->EnumNames : nullptr;
}

void FUnknownEnumNames::Find(FPropertyTypeName EnumTypeName, TArray<FName>& OutNames, bool& bOutHasFlags) const
{
	OutNames.Empty();
	bOutHasFlags = false;

	if (const FUnknownEnumNames::FInfo* Info = Enums.Find(EnumTypeName))
	{
		OutNames = Info->Names.Array();
		bOutHasFlags = Info->bHasFlags;
	}
}

void FPropertyBagRepository::ResetUnknownEnumNames(const UObject* Owner)
{
	check(Owner);

	FPropertyBagRepositoryLock LockRepo(this);
	FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner);
	if (BagData)
	{
		delete BagData->EnumNames;
		BagData->EnumNames = nullptr;
	}
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

UObject* FPropertyBagRepository::DuplicateInstanceDataObject(UObject* SourceOwner, UObject* DestOwner)
{
	if (FPropertyBagAssociationData* SourceData = AssociatedData.Find(SourceOwner))
	{
		check(SourceData->InstanceDataObject);
		FPropertyBagAssociationData& DestData = AssociatedData.FindOrAdd(DestOwner);
		ensure(!DestData.InstanceDataObject);

		// get outer pointer for new IDO
		TObjectPtr<UObject>* OuterPtr = nullptr;
		if (FPropertyBagAssociationData* OuterData = AssociatedData.Find(DestOwner->GetOuter()))
		{
			OuterPtr = &OuterData->InstanceDataObject;
		}
		if (!OuterPtr || !*OuterPtr)
		{
			OuterPtr = &Namespaces.FindOrAdd(DestOwner->GetOuter());
			if (*OuterPtr == nullptr)
			{
				*OuterPtr = CreatePackage(nullptr); // TODO: replace with dummy object
			}
		}

		// construct InstanceDataObject
		FStaticConstructObjectParameters Params(SourceData->InstanceDataObject->GetClass());
		Params.SetFlags |= EObjectFlags::RF_Transactional;
		Params.Name = DestOwner->GetFName();
		Params.Outer = *OuterPtr;
		DestData.InstanceDataObject = StaticConstructObject_Internal(Params);
		InstanceDataObjectToOwner.Add(DestData.InstanceDataObject, DestOwner);

		CopyTaggedProperties(SourceData->InstanceDataObject, DestData.InstanceDataObject);

		DestData.bNeedsFixup = SourceData->bNeedsFixup;
		return DestData.InstanceDataObject;
	}
	return nullptr;
}

void FPropertyBagRepository::PostLoadInstanceDataObject(const UObject* Owner)
{
	// fixups may have been applied to the instance during PostLoad and they need to be copied to its IDO
	FPropertyBagRepositoryLock LockRepo(this);
	if (FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner))
	{
		if (BagData->InstanceDataObject)
		{
			// copy data from owner to IDO
			CopyTaggedProperties(Owner, BagData->InstanceDataObject);
		}
	}
}

// TODO: Remove this? Bag destruction to be handled entirely via UObject::BeginDestroy() (+ FPropertyBagProperty destructor)?
void FPropertyBagRepository::DestroyOuterBag(const UObject* Owner)
{
	FPropertyBagRepositoryLock LockRepo(this);
	RemoveAssociationUnsafe(Owner);
}

bool FPropertyBagRepository::RequiresFixup(const UObject* Object, bool bIncludeOuter) const
{
	FPropertyBagRepositoryLock LockRepo(this);

	const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
	bool bResult = BagData ? BagData->bNeedsFixup : false;
	if (!bResult && bIncludeOuter)
	{
		ForEachObjectWithOuterBreakable(Object,
			[&bResult, this](UObject* Object) 
			{
				if (const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object); 
					BagData && BagData->bNeedsFixup)
				{
					bResult = true;
					return false;
				}
				return true;
			}, true);
	}
	return bResult;
}


void FPropertyBagRepository::MarkAsFixedUp(const UObject* Object)
{
	FPropertyBagRepositoryLock LockRepo(this);
	if (FPropertyBagAssociationData* BagData = AssociatedData.Find(Object))
	{
		BagData->bNeedsFixup = false;
	}
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

void FPropertyBagRepository::FindNestedInstanceDataObject(const UObject* Owner, bool bRequiresFixupOnly, TFunctionRef<void(UObject*)> Callback)
{
	FPropertyBagRepositoryLock LockRepo(this);

	if (const FPropertyBagAssociationData* BagData = AssociatedData.Find(Owner); 
		BagData && BagData->InstanceDataObject && (!bRequiresFixupOnly || BagData->bNeedsFixup))
	{
		Callback(BagData->InstanceDataObject);
	}

	ForEachObjectWithOuter(Owner,
		[this, bRequiresFixupOnly, Callback](UObject* Object)
		{
			if (const FPropertyBagAssociationData* BagData = AssociatedData.Find(Object);
				BagData && BagData->InstanceDataObject && (!bRequiresFixupOnly || BagData->bNeedsFixup))
			{
				Callback(BagData->InstanceDataObject);
			}
		}, true);
}

const UObject* FPropertyBagRepository::FindInstanceForDataObject(const UObject* InstanceDataObject) const
{
	FPropertyBagRepositoryLock LockRepo(this);
	const UObject* const* Owner = InstanceDataObjectToOwner.Find(InstanceDataObject);
	return Owner ? *Owner : nullptr;
}

bool FPropertyBagRepository::WasPropertyValueSerialized(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex)
{
	return UE::WasPropertyValueSerialized(Struct, StructData, Property, ArrayIndex);
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

	FPropertyBagPlaceholderTypeRegistry::Get().AddReferencedObjects(Collector);
}

FString FPropertyBagRepository::GetReferencerName() const
{
	return TEXT("FPropertyBagRepository");
}

void FPropertyBagRepository::CreateInstanceDataObjectUnsafe(UObject* Owner, FPropertyBagAssociationData& BagData, FArchive* Archive)
{
	check(!BagData.InstanceDataObject);	// No repeated calls
	const FPropertyPathNameTree* PropertyTree = BagData.Tree;
	const FUnknownEnumNames* EnumNames = BagData.EnumNames;
	// construct InstanceDataObject class
	// TODO: should we put the InstanceDataObject or it's class in a package?
	const UClass* InstanceDataObjectClass = CreateInstanceDataObjectClass(PropertyTree, EnumNames, Owner->GetClass(), GetTransientPackage());

	BagData.bNeedsFixup = StructContainsLooseProperties(InstanceDataObjectClass);

	TObjectPtr<UObject>* OuterPtr = nullptr;
	if (FPropertyBagAssociationData* OuterData = AssociatedData.Find(Owner->GetOuter()))
	{
		OuterPtr = &OuterData->InstanceDataObject;
	}
	if (!OuterPtr || !*OuterPtr)
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
		OldIDO->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
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
	TGuardValue<bool> ScopedTrackSerializedProperties(LoadContext->bTrackSerializedProperties, true);
	// enable impersonation so that the IDO gets loaded instead of Owner
	TGuardValue<bool> ScopedImpersonateProperties(LoadContext->bImpersonateProperties, true);

	FLinkerLoad* Linker = Owner->GetLinker();
	if (Archive && Archive != Linker)
	{
		// re-deserialize Owner but redirect it into the IDO instead using impersonation
		{
			FGuardValue_Bitfield(Archive->ArMergeOverrides, true);
			Owner->Serialize(*Archive);
		}
		
		// copy data from owner to IDO
		CopyTaggedProperties(Owner, BagData.InstanceDataObject);
	}
	else if (Linker)
	{
		Owner->SetFlags(RF_NeedLoad);
		{
			TGuardValue<bool> ScopedSkipKnownProperties(Linker->bSkipKnownProperties, true);
			FGuardValue_Bitfield(Linker->ArMergeOverrides, true);
			Linker->Preload(Owner);
		}

		// copy data from owner to IDO
		CopyTaggedProperties(Owner, BagData.InstanceDataObject);
	}
	else
	{
		ensureMsgf(BagData.Tree == nullptr,
			TEXT("Linker missing when generating IDO for an object with unknown properties. The unknown properties will be lost. Path: %s"),
			*Owner->GetPathName());
		// copy data from owner to IDO
		CopyTaggedProperties(Owner, BagData.InstanceDataObject);
	}
}

FScopedIDOSerializationContext::FScopedIDOSerializationContext(UObject* InObject, FArchive& InArchive)
	: Archive(&InArchive)
	, Object(InObject)
	, PreSerializeOffset(InArchive.Tell())
{
	FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
	bool bHasIDOSupport = IsInstanceDataObjectSupportEnabled(Object);

	bool bHasReinstancedClass = InObject->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists);
	bCreateIDO = bHasIDOSupport && !SerializeContext->bImpersonateProperties && Archive->IsLoading() && !bHasReinstancedClass;

	if (bHasIDOSupport)
	{
		if (Archive->IsLoading())
		{
			// Enable creation of a property path name tree to track any property that does not match the current class schema,
			// except when impersonation is enabled because that implies we are deserializing an IDO.
			ScopedTrackSerializedPropertyPath.Emplace(SerializeContext->bTrackSerializedPropertyPath, bCreateIDO);
			ScopedSerializeUnknownProperties.Emplace(SerializeContext->bTrackUnknownProperties, bCreateIDO);
			ScopedSerializeUnknownEnumNames.Emplace(SerializeContext->bTrackUnknownEnumNames, bCreateIDO);
			ScopedSerializedObject.Emplace(SerializeContext->SerializedObject, Object);

			// Enable tracking of initialized properties when loading an IDO, which is implied by impersonation being enabled.
			const bool bLoadingIDO = bHasIDOSupport && SerializeContext->bImpersonateProperties;
			ScopedTrackInitializedProperties.Emplace(SerializeContext->bTrackInitializedProperties, bLoadingIDO);
			ScopedTrackSerializedProperties.Emplace(SerializeContext->bTrackSerializedProperties, bLoadingIDO);
		}
		else
		{
			ScopedImpersonateProperties.Emplace(SerializeContext->bImpersonateProperties, bHasIDOSupport);
		}
	}
}

FScopedIDOSerializationContext::FScopedIDOSerializationContext(UObject* InObject, bool bImpersonate)
	: bCreateIDO(false)
	, Archive(nullptr)
	, Object(InObject)
	, PreSerializeOffset(0)
{
	FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
	bool bHasIDOSupport = IsInstanceDataObjectSupportEnabled(Object);
	if (bHasIDOSupport)
	{
		ScopedImpersonateProperties.Emplace(SerializeContext->bImpersonateProperties, bImpersonate);
	}
}

FScopedIDOSerializationContext::FScopedIDOSerializationContext(bool bImpersonate)
	: bCreateIDO(false)
	, Archive(nullptr)
	, Object(nullptr)
	, PreSerializeOffset(0)
{
	FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
	bool bHasIDOSupport = IsInstanceDataObjectSupportEnabled();
	if (bHasIDOSupport)
	{
		ScopedImpersonateProperties.Emplace(SerializeContext->bImpersonateProperties, bImpersonate);
	}
}

FScopedIDOSerializationContext::~FScopedIDOSerializationContext()
{
	if (bCreateIDO)
	{
		FinishCreatingInstanceDataObject();
	}
}

void FScopedIDOSerializationContext::FinishCreatingInstanceDataObject() const
{
	if (Archive == Object->GetLinker())
	{
		// when using the linker, the repository will handle offsets
		FPropertyBagRepository::Get().CreateInstanceDataObject(Object, Archive);
	}
	else
	{
		check(Archive);
		const int64 PostSerializeOffset = Archive->Tell();
	
		// CreateInstanceDataObject will re-call DstObject->Serialize(*this) so set the seek pointer back before DestObject in the archive
		Archive->Seek(PreSerializeOffset);
			
		FPropertyBagRepository::Get().CreateInstanceDataObject(Object, Archive);

		// make sure seek pointer is back to where it should be
		if (!ensure(Archive->Tell() == PostSerializeOffset))
		{
			// for some reason CreateInstanceDataObject read a different amount of data than expected... reset seek pointer back to where it should be
			Archive->Seek(PostSerializeOffset);
		}
	}
}

// Not sure this is necessary.
void FPropertyBagRepository::ShrinkMaps()
{
	FPropertyBagRepositoryLock LockRepo(this);
	AssociatedData.Compact();
	InstanceDataObjectToOwner.Compact();
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderType(const UStruct* Type)
{
	if (!Type)
	{
		return false;
	}

	return FPropertyBagPlaceholderTypeRegistry::Get().Contains(Type);
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderObject(const UObject* Object)
{
	if (!Object)
	{
		return false;
	}

	return IsPropertyBagPlaceholderType(Object->GetClass());
}

namespace Private
{
#if WITH_EDITOR
	static bool bEnablePropertyBagPlaceholderObjectSupport = 0;
	static FAutoConsoleVariableRef CVarEnablePropertyBagPlaceholderObjectSupport(
		TEXT("SceneGraph.EnablePropertyBagPlaceholderObjectSupport"),
		bEnablePropertyBagPlaceholderObjectSupport,
		TEXT("If true, allows placeholder types to be created in place of missing types in order to redirect serialization into a property bag."),
		ECVF_Default
	);
#endif
}

bool FPropertyBagRepository::IsPropertyBagPlaceholderObjectSupportEnabled()
{
#if WITH_EDITOR && UE_WITH_OBJECT_HANDLE_TYPE_SAFETY
	static bool bIsInitialized = false;
	static bool bForceDisabled = false;
	if (!bIsInitialized)
	{
		Private::bEnablePropertyBagPlaceholderObjectSupport |= FParse::Param(FCommandLine::Get(), TEXT("WithPropertyBagPlaceholderObjects"));
		Private::CVarEnablePropertyBagPlaceholderObjectSupport->OnChangedDelegate().AddLambda([](IConsoleVariable* CVar)
		{
			bForceDisabled = !CVar->GetBool();
		});

		bIsInitialized = true;
	}
	
	return Private::bEnablePropertyBagPlaceholderObjectSupport || (IsInstanceDataObjectSupportEnabled() && !bForceDisabled);
#else
	return false;
#endif
}

UStruct* FPropertyBagRepository::CreatePropertyBagPlaceholderType(UObject* Outer, UClass* Class, FName Name, EObjectFlags Flags, UStruct* SuperStruct)
{
	// Generate and link a new type object using the given SuperStruct as its base.
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

		// Required by garbage collection for class types.
		PlaceholderTypeAsClass->AssembleReferenceTokenStream();
	}

	// Use the property bag repository for now to manage property bag placeholder types (e.g. object lifetime).
	// Note: The object lifetime of instances of this type will rely on existing references that are serialized.
	FPropertyBagPlaceholderTypeRegistry::Get().Add(PlaceholderType);

	return PlaceholderType;
}

void FPropertyBagRepository::RemovePropertyBagPlaceholderType(UStruct* PlaceholderType)
{
	ensure(IsPropertyBagPlaceholderType(PlaceholderType));
	FPropertyBagPlaceholderTypeRegistry::Get().Remove(PlaceholderType);
}

} // UE

#endif // WITH_EDITORONLY_DATA
