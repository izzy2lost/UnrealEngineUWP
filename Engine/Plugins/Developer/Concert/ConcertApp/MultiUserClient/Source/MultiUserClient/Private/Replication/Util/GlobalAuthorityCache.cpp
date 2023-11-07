// Copyright Epic Games, Inc. All Rights Reserved.

#include "GlobalAuthorityCache.h"

#include "Replication/AuthorityConflictSharedUtils.h"
#include "Replication/Client/RemoteReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"

namespace UE::MultiUserClient
{
	FGlobalAuthorityCache::FGlobalAuthorityCache(FReplicationClientManager& InClientManager)
		: ClientManager(InClientManager)
	{
		RegisterForClientEvents(ClientManager.GetLocalClient());

		ClientManager.OnPostRemoteClientAdded().AddRaw(this, &FGlobalAuthorityCache::OnPostRemoteClientAdded);
		ClientManager.OnPreRemoteClientRemoved().AddRaw(this, &FGlobalAuthorityCache::OnPreRemoteClientRemoved);
	}

	FGlobalAuthorityCache::~FGlobalAuthorityCache()
	{
		// FGlobalAuthorityCache is owned by FRelicationClientManager so this is strictly not needed. But we'll follow RAII in case ownership changes.
		UnregisterFromClientEvents(ClientManager.GetLocalClient());
		for (const TNonNullPtr<FRemoteReplicationClient>& RemoteClient : ClientManager.GetRemoteClients())
		{
			UnregisterFromClientEvents(*RemoteClient);
		}
		
		ClientManager.OnPostRemoteClientAdded().RemoveAll(this);
		ClientManager.OnPreRemoteClientRemoved().RemoveAll(this);
	}

	void FGlobalAuthorityCache::ForEachClientWithAuthorityOverObject(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FGuid& ClientId)> Callback) const
	{
		const TSet<FGuid>* Clients = OwnedObjectsToClients.Find(Object);
		if (!Clients)
		{
			return;
		}

		for (const FGuid& ClientId : *Clients)
		{
			if (Callback(ClientId) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	TArray<FGuid> FGlobalAuthorityCache::GetClientsWithAuthorityOverObject(const FSoftObjectPath& Object) const
	{
		TArray<FGuid> Result;
		ForEachClientWithAuthorityOverObject(Object, [&Result](const FGuid& ClientId)
		{
			Result.Add(ClientId);
			return EBreakBehavior::Continue;
		});
		return Result;
	}

	FGlobalAuthorityCache::ECanTakeAuthority FGlobalAuthorityCache::CanClientTakeAuthority(const FSoftObjectPath& Object, const FGuid& ClientId, FProcessPropertyConflict ProcessConflict) const
	{
		const FReplicationClient* Client = ClientManager.FindClient(ClientId);
		if (!ensure(Client))
		{
			return ECanTakeAuthority::NotApplicable;
		}
		
		const FReplicatedObjectInfo* ObjectInfo = Client->GetStreamSynchronizer().GetServerState().ReplicatedObjects.Find(Object);
		if (!ObjectInfo)
		{
			// Nothing to take authority over
			return ECanTakeAuthority::NotApplicable;
		}
		
		using namespace ConcertSyncCore::Replication::AuthorityConflictUtils;
		const EAuthorityConflict Conflict = EnumerateAuthorityConflicts(
			ClientId,
			Object,
			ObjectInfo->PropertySelection.ReplicatedProperties,
			*this,
			[&ProcessConflict](const FGuid& ClientId, const FGuid&, const FConcertPropertyChain& ConflictingProperty)
			{
				return ProcessConflict(ClientId, ConflictingProperty);
			});
		return Conflict == EAuthorityConflict::Allowed ? ECanTakeAuthority::Allowed : ECanTakeAuthority::Conflict;
	}

	bool FGlobalAuthorityCache::CanClientAddProperty(const FSoftObjectPath& Object, const FGuid& ClientId, const FConcertPropertyChain& Chain) const
	{
		const FReplicationClient* Client = ClientManager.FindClient(ClientId);
		if (!ensure(Client))
		{
			return false;
		}
		
		using namespace ConcertSyncCore::Replication::AuthorityConflictUtils;
		const EAuthorityConflict Conflict = EnumerateAuthorityConflicts(
			ClientId,
			Object,
			{ Chain },
			*this
			);
		return Conflict == EAuthorityConflict::Allowed;
	}

	TOptional<FGuid> FGlobalAuthorityCache::GetClientWithAuthorityOverProperty(const FSoftObjectPath& Object, const FConcertPropertyChain& Property) const
	{
		TOptional<FGuid> Result;
		ForEachClientWithAuthorityOverObject(Object, [this, &Object, &Property, &Result](const FGuid& ClientId)
		{
			const FReplicationClient* Client = ClientManager.FindClient(ClientId);
			if (!ensureMsgf(Client, TEXT("OnPreRemoteClientRemoved should have updated OwnedObjectsToClients")))
			{
				return EBreakBehavior::Continue;
			}
			
			const FReplicatedObjectInfo* ObjectInfo = Client->GetStreamSynchronizer().GetServerState().ReplicatedObjects.Find(Object);
			const bool bHasObjectRegistered = ensureMsgf(ObjectInfo, TEXT("OnStreamChanged should have updated OwnedObjectsToClients"))
				&& ObjectInfo->PropertySelection.ReplicatedProperties.Contains(Property);
			const bool bHasAuthority = Client->GetAuthoritySynchronizer().HasAuthorityOver(Object);
			if (bHasObjectRegistered && bHasAuthority)
			{
				Result = ClientId;
				return EBreakBehavior::Break;
			}
			return EBreakBehavior::Continue;
		});
		return Result;
	}

	void FGlobalAuthorityCache::RegisterForClientEvents(const FReplicationClient& Client)
	{
		const FGuid& ClientEndpointId = Client.GetEndpointId();
		Client.GetAuthoritySynchronizer().OnServerStateChanged().AddRaw(this, &FGlobalAuthorityCache::OnPostAuthorityChanged, ClientEndpointId);
		Client.GetStreamSynchronizer().OnServerStateChanged().AddRaw(this, &FGlobalAuthorityCache::OnStreamChanged, ClientEndpointId);
	}

	void FGlobalAuthorityCache::UnregisterFromClientEvents(const FReplicationClient& Client) const
	{
		Client.GetAuthoritySynchronizer().OnServerStateChanged().RemoveAll(this);
		Client.GetStreamSynchronizer().OnServerStateChanged().RemoveAll(this);
	}

	void FGlobalAuthorityCache::AddClient(const FGuid& ClientId)
	{
		const FReplicationClient* Client = ClientManager.FindClient(ClientId);
		if (!ensure(Client))
		{
			return;
		}
		
		const FObjectReplicationMap& ClientObjectMap = Client->GetStreamSynchronizer().GetServerState();
		const IClientAuthoritySynchronizer& ClientAuthority = Client->GetAuthoritySynchronizer();
		for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& StreamContents : ClientObjectMap.ReplicatedObjects)
		{
			const FSoftObjectPath& Object = StreamContents.Key;
			if (ClientAuthority.HasAuthorityOver(Object))
			{
				OwnedObjectsToClients.FindOrAdd(Object).Add(ClientId);
			}
		}
	}

	void FGlobalAuthorityCache::RemoveClient(const FGuid& ClientId)
	{
		for (auto It = OwnedObjectsToClients.CreateIterator(); It; ++It)
		{
			It->Value.Remove(ClientId);
			if (It->Value.IsEmpty())
			{
				It.RemoveCurrent();
			}
		}
	}

	void FGlobalAuthorityCache::ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FGuid& StreamId, const FObjectReplicationMap& ReplicationMap)> Callback) const
	{
		const FReplicationClient* Client = ClientManager.FindClient(ClientEndpointId);
		if (!ensure(Client))
		{
			return;
		}

		const IClientStreamSynchronizer& StreamSynchronizer = Client->GetStreamSynchronizer();
		Callback(StreamSynchronizer.GetStreamId(), StreamSynchronizer.GetServerState());
	}

	void FGlobalAuthorityCache::ForEachSendingClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback) const
	{
		auto ProcessClient = [&Callback](const FReplicationClient& Client)
		{
			return Client.GetAuthoritySynchronizer().HasAnyAuthority()
				? Callback(Client.GetEndpointId())
				:EBreakBehavior::Continue;
		};

		if (ProcessClient(ClientManager.GetLocalClient()) == EBreakBehavior::Break)
		{
			return;
		}
		for (const TNonNullPtr<FRemoteReplicationClient>& RemoteClient : ClientManager.GetRemoteClients())
		{
			if (ProcessClient(*RemoteClient) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	bool FGlobalAuthorityCache::HasAuthority(const FGuid& ClientId, const FGuid& StreamId, const FSoftObjectPath& ObjectPath) const
	{
		return GetClientsWithAuthorityOverObject(ObjectPath).Contains(ClientId);
	}
}
