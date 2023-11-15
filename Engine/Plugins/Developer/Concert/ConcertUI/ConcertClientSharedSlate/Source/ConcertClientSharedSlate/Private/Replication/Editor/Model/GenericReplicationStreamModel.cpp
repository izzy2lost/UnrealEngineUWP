// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericReplicationStreamModel.h"

#include "Replication/PropertyChainUtils.h"
#include "Replication/Data/ObjectReplicationMap.h"
#include "Replication/Settings/ConcertReplicationEditorSettings.h"

#include "Algo/AllOf.h"

namespace UE::ConcertClientSharedSlate
{
	FGenericReplicationStreamModel::FGenericReplicationStreamModel(TAttribute<FObjectReplicationMap*> ReplicationMapAttribute)
		: ReplicationMapAttribute(MoveTemp(ReplicationMapAttribute))
	{}

	FSoftClassPath FGenericReplicationStreamModel::GetObjectClass(const FSoftObjectPath& Object) const
	{
		const FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		if (!ensure(ReplicationMap))
		{
			return {};
		}
		
		const FReplicatedObjectInfo* AssignedProperties = ReplicationMap->ReplicatedObjects.Find(Object);
		return AssignedProperties
			? AssignedProperties->ClassPath
			: FSoftClassPath{};
	}

	bool FGenericReplicationStreamModel::ContainsObjects(const TSet<FSoftObjectPath>& Objects) const
	{
		const FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		return ensure(ReplicationMap)
			&& Algo::AllOf(Objects, [this, ReplicationMap](const FSoftObjectPath& ObjectPath){ return ReplicationMap->ReplicatedObjects.Contains(ObjectPath); });
	}

	bool FGenericReplicationStreamModel::ContainsProperties(const FSoftObjectPath& Object, const TSet<FConcertPropertyChain>& Properties) const
	{
		const FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		if (!ensure(ReplicationMap))
		{
			return false;
		}

		const FReplicatedObjectInfo* ObjectInfo = ReplicationMap->ReplicatedObjects.Find(Object);
		return ObjectInfo
			&& Algo::AllOf(Properties, [ObjectInfo](const FConcertPropertyChain& Property){ return ObjectInfo->PropertySelection.ReplicatedProperties.Contains(Property); });
	}

	bool FGenericReplicationStreamModel::ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const
	{
		const FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		if (!ensure(ReplicationMap))
		{
			return false;
		}

		for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& ObjectMap: ReplicationMap->ReplicatedObjects)
		{
			if (Delegate(ObjectMap.Key) == EBreakBehavior::Break)
			{
				return true;
			}
		}

		return !ReplicationMap->ReplicatedObjects.IsEmpty();
	}

	bool FGenericReplicationStreamModel::ForEachProperty(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Parent)> Delegate) const
	{
		const FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		if (!ensure(ReplicationMap))
		{
			return false;
		}

		const FReplicatedObjectInfo* AssignedProperties = ReplicationMap->ReplicatedObjects.Find(Object);
		if (!AssignedProperties)
		{
			return false;
		}

		for (const FConcertPropertyChain& ReplicatedPropertyInfo : AssignedProperties->PropertySelection.ReplicatedProperties)
		{
			if (Delegate(ReplicatedPropertyInfo) == EBreakBehavior::Break)
			{
				return true;
			}
		}
		return !AssignedProperties->PropertySelection.ReplicatedProperties.IsEmpty();
	}

	void FGenericReplicationStreamModel::AddObjects(TConstArrayView<UObject*> Objects)
	{
		FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		if (!ensure(ReplicationMap) || Objects.IsEmpty())
		{
			return;
		}
		
		TSet<UObject*> ObjectsNotAdded;
		for (UObject* Object : Objects)
		{
			const FSoftObjectPath ObjectPath = Object;
			if (ensureAlways(Object) && !ReplicationMap->ReplicatedObjects.Contains(ObjectPath))
			{
				FReplicatedObjectInfo& ObjectInfo = ReplicationMap->ReplicatedObjects.Add(ObjectPath);
				ObjectInfo.ClassPath = Object->GetClass();
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

	void FGenericReplicationStreamModel::RemoveObjects(TConstArrayView<FSoftObjectPath> Objects)
	{
		FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		if (!ensure(ReplicationMap) || Objects.IsEmpty())
		{
			return;
		}
		
		TSet<FSoftObjectPath> ObjectsNotRemoved;
		for (const FSoftObjectPath& Object : Objects)
		{
			const bool bRemoved = ReplicationMap->ReplicatedObjects.Remove(Object) != 0;
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

	void FGenericReplicationStreamModel::AddProperties(const FSoftObjectPath& Object, TConstArrayView<FConcertPropertyChain> Properties)
	{
		FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		if (!ensure(ReplicationMap))
		{
			return;
		}
		
		FReplicatedObjectInfo* AssignedProperties = ReplicationMap->ReplicatedObjects.Find(Object);
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
		TArray<FConcertPropertyChain>& ReplicatedProperties = AssignedProperties->PropertySelection.ReplicatedProperties;
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
					bAddedAtLeastOne |= AssignedProperties->PropertySelection.ReplicatedProperties.AddUnique(Property) != INDEX_NONE;
				}
				return EBreakBehavior::Continue;
			});
		}

		if (bAddedAtLeastOne)
		{
			OnPropertiesChangedDelegate.Broadcast();
		}
	}

	void FGenericReplicationStreamModel::RemoveProperties(const FSoftObjectPath& Object, TConstArrayView<FConcertPropertyChain> Properties)
	{
		FObjectReplicationMap* ReplicationMap = ReplicationMapAttribute.Get();
		if (!ensure(ReplicationMap))
		{
			return;
		}
		
		FReplicatedObjectInfo* AssignedProperties = ReplicationMap->ReplicatedObjects.Find(Object);
		if (!AssignedProperties)
		{
			return;
		}

		UClass* Class = AssignedProperties->ClassPath.TryLoadClass<UObject>();
		int32 NumRemoved = 0;
		for (const FConcertPropertyChain& RemovedProperty : Properties)
		{
			NumRemoved += AssignedProperties->PropertySelection.ReplicatedProperties.Remove(RemovedProperty);

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
					NumRemoved += AssignedProperties->PropertySelection.ReplicatedProperties.Remove(Property);
				}
				return EBreakBehavior::Continue;
			});
		}
		
		if (NumRemoved > 0)
		{
			OnPropertiesChangedDelegate.Broadcast();
		}
	}
}
