// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssignedClientsModel.h"

#include "Replication/Client/Online/OnlineClient.h"
#include "Replication/Client/Online/OnlineClientManager.h"
#include "Replication/Editor/Model/Object/IObjectHierarchyModel.h"

#include "Containers/Array.h"
#include "Misc/Guid.h"

namespace UE::MultiUserClient::Replication::MultiStreamColumns::Private
{
	template<typename TCallback> requires std::is_invocable_v<TCallback, const FGuid&>
	static void EnumerateOwningOnlineClients(
		const FOnlineClientManager& ClientManager,
		const FSoftObjectPath& ObjectPath,
		TCallback&& Callback
		) 
	{
		ClientManager.ForEachClient([&ObjectPath, &Callback](const FOnlineClient& ReplicationClient)
		{
			const bool bHasProperties = ReplicationClient.GetStreamSynchronizer().GetServerState().HasProperties(ObjectPath);
			if (bHasProperties)
			{
				Callback(ReplicationClient.GetEndpointId());
			}
			return EBreakBehavior::Continue;
		});
	}
}

namespace UE::MultiUserClient::Replication::MultiStreamColumns
{
	FAssignedClientsModel::FAssignedClientsModel(
		const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy,
		FOnlineClientManager& InClientManager
		)
		: ObjectHierarchy(ObjectHierarchy)
		, ClientManager(InClientManager)
	{
		ClientManager.OnRemoteClientsChanged().AddRaw(this, &FAssignedClientsModel::BroadcastOwnershipChanged);
		ClientManager.GetAuthorityCache().OnCacheChanged().AddRaw(this, &FAssignedClientsModel::OnClientChanged);
	}

	FAssignedClientsModel::~FAssignedClientsModel()
	{
		ClientManager.OnRemoteClientsChanged().RemoveAll(this);
		ClientManager.GetAuthorityCache().OnCacheChanged().RemoveAll(this);
	}

	TArray<FGuid> FAssignedClientsModel::GetAssignedClients(const FSoftObjectPath& ObjectPath) const
	{
		TArray<FGuid> ClientsWithOwnership;
		const auto ProcessObject = [this, &ClientsWithOwnership](const FSoftObjectPath& ObjectPath)
		{
			Private::EnumerateOwningOnlineClients(ClientManager, ObjectPath, [&ClientsWithOwnership](const FGuid& ClientId)
			{
				ClientsWithOwnership.AddUnique(ClientId);
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
}
