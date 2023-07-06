// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertySelectionAssetModel.h"

#include "Replication/PropertyChainUtils.h"
#include "Settings/DefaultPropertySelection.h"
#include "Settings/MultiUserReplicationSettings.h"

#include "Algo/AllOf.h"
#include "ScopedTransaction.h"
#include "Misc/TransactionObjectEvent.h"

#define LOCTEXT_NAMESPACE "FPropertySelectionAssetModel"

namespace UE::MultiUserReplicationEditor
{
	FPropertySelectionAssetModel::FPropertySelectionAssetModel(UMultiUserPropertyReplicationSelection& Object)
		: Asset(&Object)
	{}
	
	uint32 FPropertySelectionAssetModel::GetNumReplicatedObjects() const
	{
		return ensure(Asset.IsValid())
			? Asset->ReplicationMap.ReplicatedObjects.Num()
			: 0;
	}

	uint32 FPropertySelectionAssetModel::GetNumProperties(const FSoftObjectPath& Object) const
	{
		if (!ensure(Asset.IsValid()))
		{
			return false;
		}

		const FReplicatedObjectInfo* AssignedProperties = Asset->ReplicationMap.ReplicatedObjects.Find(Object);
		return AssignedProperties
			? AssignedProperties->ReplicatedProperties.ReplicatedProperties.Num()
			: 0;
	}

	FSoftClassPath FPropertySelectionAssetModel::GetObjectClass(const FSoftObjectPath& Object) const
	{
		if (!ensure(Asset.IsValid()))
		{
			return {};
		}
		
		const FReplicatedObjectInfo* AssignedProperties = Asset->ReplicationMap.ReplicatedObjects.Find(Object);
		return AssignedProperties
			? AssignedProperties->ClassPath
			: FSoftClassPath{};
	}

	bool FPropertySelectionAssetModel::ContainsObjects(const TSet<FSoftObjectPath>& Objects) const
	{
		return ensure(Asset.IsValid())
			&& Algo::AllOf(Objects, [this](const FSoftObjectPath& ObjectPath){ return Asset->ReplicationMap.ReplicatedObjects.Contains(ObjectPath); });
	}

	bool FPropertySelectionAssetModel::ContainsProperties(const FSoftObjectPath& Object, const TSet<FConcertPropertyChain>& Properties) const
	{
		if (!ensure(Asset.IsValid()))
		{
			return false;
		}

		const FReplicatedObjectInfo* ObjectInfo = Asset->ReplicationMap.ReplicatedObjects.Find(Object);
		return ObjectInfo
			&& Algo::AllOf(Properties, [ObjectInfo](const FConcertPropertyChain& Property){ return ObjectInfo->ReplicatedProperties.ReplicatedProperties.Contains(Property); });
	}

	bool FPropertySelectionAssetModel::ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const
	{
		if (!ensure(Asset.IsValid()))
		{
			return false;
		}

		for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& ObjectMap: Asset->ReplicationMap.ReplicatedObjects)
		{
			if (Delegate(ObjectMap.Key) == EBreakBehavior::Break)
			{
				return true;
			}
		}

		return !Asset->ReplicationMap.ReplicatedObjects.IsEmpty();
	}

	bool FPropertySelectionAssetModel::ForEachProperty(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Parent)> Delegate) const
	{
		if (!ensure(Asset.IsValid()))
		{
			return false;
		}

		const FReplicatedObjectInfo* AssignedProperties = Asset->ReplicationMap.ReplicatedObjects.Find(Object);
		if (!AssignedProperties)
		{
			return false;
		}

		for (const FConcertPropertyChain& ReplicatedPropertyInfo : AssignedProperties->ReplicatedProperties.ReplicatedProperties)
		{
			if (Delegate(ReplicatedPropertyInfo) == EBreakBehavior::Break)
			{
				return true;
			}
		}
		return !AssignedProperties->ReplicatedProperties.ReplicatedProperties.IsEmpty();
	}

	void FPropertySelectionAssetModel::AddObjects(TArrayView<UObject*> Objects)
	{
		if (!ensure(Asset.IsValid()) || Objects.IsEmpty())
		{
			return;
		}

		const FScopedTransaction Transaction(LOCTEXT("AddObjects", "Add replicated objects"));
		Asset->Modify();
		
		TSet<UObject*> ObjectsNotAdded;
		for (UObject* Object : Objects)
		{
			const FSoftObjectPath ObjectPath = Object;
			if (ensureAlways(Object) && !Asset->ReplicationMap.ReplicatedObjects.Contains(ObjectPath))
			{
				FReplicatedObjectInfo& ObjectInfo = Asset->ReplicationMap.ReplicatedObjects.Add(ObjectPath);
				ObjectInfo.ClassPath = Object->GetClass();
				UMultiUserReplicationSettings::Get()->AddDefaultPropertiesFromSettings(ObjectInfo, *Object->GetClass());
			}
			else
			{
				ObjectsNotAdded.Add(Object);
			}
		}
		
		if (LIKELY(ObjectsNotAdded.IsEmpty()))
		{
			OnObjectsChangedDelegate.Broadcast(Objects, {}, EReplicatedObjectChangeReason::ChangedDirectly);
		}
		else if (ObjectsNotAdded.Num() < Objects.Num())
		{
			// Uncommon case so it is ok if it is suboptimal
			TArray<UObject*> Added;
			for (UObject* Object : Objects)
			{
				if (!ObjectsNotAdded.Contains(Object))
				{
					Added.Add(Object);
				}
			}
			OnObjectsChangedDelegate.Broadcast(Added, {}, EReplicatedObjectChangeReason::ChangedDirectly);
		}
	}

	void FPropertySelectionAssetModel::RemoveObjects(TArrayView<FSoftObjectPath> Objects)
	{
		if (!ensure(Asset.IsValid()) || Objects.IsEmpty())
		{
			return;
		}

		const FScopedTransaction Transaction(LOCTEXT("RemoveObjects", "Remove replicated objects"));
		Asset->Modify();
		
		TSet<FSoftObjectPath> ObjectsNotRemoved;
		for (const FSoftObjectPath& Object : Objects)
		{
			const bool bRemoved = Asset->ReplicationMap.ReplicatedObjects.Remove(Object) != 0;
			if (!bRemoved)
			{
				ObjectsNotRemoved.Add(Object);
			}
		}
		
		if (LIKELY(ObjectsNotRemoved.IsEmpty()))
		{
			OnObjectsChangedDelegate.Broadcast({}, Objects, EReplicatedObjectChangeReason::ChangedDirectly);
		}
		else if (ObjectsNotRemoved.Num() < Objects.Num())
		{
			// Uncommon case so it is ok if it is suboptimal
			TArray<FSoftObjectPath> Removed;
			for (const FSoftObjectPath& Object : Objects)
			{
				if (!ObjectsNotRemoved.Contains(Object))
				{
					Removed.Add(Object);
				}
			}
			OnObjectsChangedDelegate.Broadcast({}, Removed, EReplicatedObjectChangeReason::ChangedDirectly);
		}
	}

	void FPropertySelectionAssetModel::AddProperties(const FSoftObjectPath& Object, TArrayView<FConcertPropertyChain> Properties)
	{
		if (!ensure(Asset.IsValid()))
		{
			return;
		}
		
		const FScopedTransaction Transaction(LOCTEXT("AddProperties", "Add replicated properties"));
		Asset->Modify();
		
		FReplicatedObjectInfo* AssignedProperties = Asset->ReplicationMap.ReplicatedObjects.Find(Object);
		if (!AssignedProperties)
		{
			return;
		}

		UClass* Class = AssignedProperties->ClassPath.TryLoadClass<UObject>();
		if (!Class)
		{
			return;
		}

		bool bAddedAtLeastOne = false;
		TArray<FConcertPropertyChain>& ReplicatedProperties = AssignedProperties->ReplicatedProperties.ReplicatedProperties;
		ReplicatedProperties.Reserve(ReplicatedProperties.Num() + Properties.Num());
		for (const FConcertPropertyChain& AddedProperty : Properties)
		{
			bAddedAtLeastOne |= ReplicatedProperties.AddUnique(AddedProperty) != INDEX_NONE;

			// Parent properties must also be added
			// Not exactly efficient to iterate through the hierarchy for every removed item but it should be fine... Properties.Num() == 1 is the most common case
			ConcertSyncCore::PropertyChain::ForEachReplicatableConcertProperty(*Class, [&bAddedAtLeastOne, &AssignedProperties, &AddedProperty](FConcertPropertyChain&& Property)
			{
				if (Property.IsParentOf(AddedProperty))
				{
					bAddedAtLeastOne |= AssignedProperties->ReplicatedProperties.ReplicatedProperties.AddUnique(Property) != INDEX_NONE;
				}
				return EBreakBehavior::Continue;
			});
		}

		if (bAddedAtLeastOne)
		{
			OnPropertiesChangedDelegate.Broadcast();
		}
	}

	void FPropertySelectionAssetModel::RemoveProperties(const FSoftObjectPath& Object, TArrayView<FConcertPropertyChain> Properties)
	{
		if (!ensure(Asset.IsValid()))
		{
			return;
		}

		const FScopedTransaction Transaction(LOCTEXT("RemoveProperties", "Remove replicated properties"));
		Asset->Modify();
		
		FReplicatedObjectInfo* AssignedProperties = Asset->ReplicationMap.ReplicatedObjects.Find(Object);
		if (!AssignedProperties)
		{
			return;
		}

		UClass* Class = AssignedProperties->ClassPath.TryLoadClass<UObject>();
		int32 NumRemoved = 0;
		for (const FConcertPropertyChain& RemovedProperty : Properties)
		{
			NumRemoved += AssignedProperties->ReplicatedProperties.ReplicatedProperties.Remove(RemovedProperty);

			// Removal should not fail if the class is not available
			if (!Class)
			{
				continue;
			}

			// Child properties must also be removed
			// Not exactly efficient to iterate through the hierarchy for every removed item but it should be fine... Properties.Num() == 1 is the most common case
			ConcertSyncCore::PropertyChain::ForEachReplicatableConcertProperty(*Class, [&NumRemoved, &AssignedProperties, &RemovedProperty](FConcertPropertyChain&& Property)
			{
				if (Property.IsChildOf(RemovedProperty))
				{
					NumRemoved += AssignedProperties->ReplicatedProperties.ReplicatedProperties.Remove(Property);
				}
				return EBreakBehavior::Continue;
			});
		}
		
		if (NumRemoved > 0)
		{
			OnPropertiesChangedDelegate.Broadcast();
		}
	}

	bool FPropertySelectionAssetModel::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
	{
		if (!ensure(Asset.IsValid()))
		{
			return false;
		}
		
		for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjectContexts)
		{
			UObject* Object = TransactionObjectPair.Key;
			while (Object != nullptr)
			{
				if (Object == Asset.Get())
				{
					return true;
				}
				Object = Object->GetOuter();
			}
		}

		return false;
	}

	void FPropertySelectionAssetModel::PostUndo(bool bSuccess)
	{
		OnObjectsChangedDelegate.Broadcast({}, {}, EReplicatedObjectChangeReason::Transacted);
		OnPropertiesChangedDelegate.Broadcast();
	}

	void FPropertySelectionAssetModel::PostRedo(bool bSuccess)
	{
		OnObjectsChangedDelegate.Broadcast({}, {}, EReplicatedObjectChangeReason::Transacted);
		OnPropertiesChangedDelegate.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE