// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssignedClientsModel.h"

#include "Replication/Client/UnifiedStreamCache.h"
#include "Replication/Editor/Model/Object/IObjectHierarchyModel.h"

#include "Containers/Array.h"
#include "Misc/Guid.h"

#include <type_traits>

namespace UE::MultiUserClient::Replication::MultiStreamColumns
{
	FAssignedClientsModel::FAssignedClientsModel(
		const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy,
		FUnifiedStreamCache& InStreamCache
		)
		: ObjectHierarchy(ObjectHierarchy)
		, StreamCache(InStreamCache)
	{
		StreamCache.OnCacheChanged().AddRaw(this, &FAssignedClientsModel::BroadcastOwnershipChanged);
	}

	FAssignedClientsModel::~FAssignedClientsModel()
	{
		StreamCache.OnCacheChanged().RemoveAll(this);
	}

	TArray<FGuid> FAssignedClientsModel::GetAssignedClients(const FSoftObjectPath& ObjectPath) const
	{
		TArray<FGuid> ClientsWithOwnership;

		const auto ProcessObject = [this, &ClientsWithOwnership](const FSoftObjectPath& ObjectPath)
		{
			StreamCache.EnumerateClientsWithObject(ObjectPath, [&ClientsWithOwnership](const FGuid& ClientId)
			{
				ClientsWithOwnership.AddUnique(ClientId);
				return EBreakBehavior::Continue;
			});
		};
			
		ProcessObject(ObjectPath);
		ObjectHierarchy.ForEachChildRecursive(
			TSoftObjectPtr{ ObjectPath },
			[&ProcessObject](const TSoftObjectPtr<>&, const TSoftObjectPtr<>& ChildObject, ConcertSharedSlate::EChildRelationship)
			{
				ProcessObject(ChildObject.GetUniqueID());
				return EBreakBehavior::Continue;
			});
		
		return ClientsWithOwnership;
	}

	void FAssignedClientsModel::OnClientChanged(const FGuid&) const
	{
		BroadcastOwnershipChanged();
	}

	void FAssignedClientsModel::BroadcastOwnershipChanged() const
	{
		OnOwnershipChangedDelegate.Broadcast();
	}
}
