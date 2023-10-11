// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Assets/MultiUserReplicationSessionPreset.h"
#include "ConcertMessageData.h"
#include "LocalReplicationClient.h"
#include "ReplicationClient.h"
#include "Replication/Util/RegularQueryService.h"

#include "UObject/GCObject.h"
#include "Templates/UnrealTemplate.h"

enum class EConcertClientStatus : uint8;

class IConcertClientSession;
class IConcertSyncClient;

struct FConcertSessionClientInfo;

namespace UE::ConcertClientSharedSlate
{
	class IEditableObjectToPropertiesModel;
}

namespace UE::MultiUserClient
{
	class FRemoteReplicationClient;
	class FReplicationClient;
	class FLocalStreamChangeTracker;

	/**
	 * Keeps track of connected clients synchronizing their stream data in a UMultiUserReplicationSessionPreset.
	 * This object only exists for as long as the local client is in a Concert session.
	 */
	class FReplicationClientManager
		: public FGCObject
		, public FNoncopyable
	{
	public:
		
		/**
		 * @param InClient The local client. Outlives this objects.
		 * @param InSession The session to observe. Outlives this objects.
		 */
		FReplicationClientManager(TSharedRef<IConcertSyncClient> InClient, TSharedRef<IConcertClientSession> InSession);
		virtual ~FReplicationClientManager() override;

		const FLocalReplicationClient& GetLocalClient() const { return LocalClient; }
		FLocalReplicationClient& GetLocalClient() { return LocalClient; }
		const TArray<FRemoteReplicationClient>& GetRemoteClients() const { return RemoteClients; }

		/** Util for finding a remote client by its EndpointId. */
		const FRemoteReplicationClient* FindRemoteClient(const FGuid& EndpointId) const;
		FRemoteReplicationClient* FindRemoteClient(const FGuid& EndpointId)
		{
			const FReplicationClientManager* ConstThis = this;
			return const_cast<FRemoteReplicationClient*>(ConstThis->FindRemoteClient(EndpointId));
		}
		
		DECLARE_MULTICAST_DELEGATE(FRemoteClientsChanged);
		/** Called when RemoteClients changes. */
		FRemoteClientsChanged& OnRemoteClientsChanged() { return OnRemoteClientsChangedDelegate; }
		
		//~ Begin FGCObject Interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FReplicationStreamSynchronizer"); }
		//~ End FGCObject Interface
		
	private:
		
		/** The state of the server is synched up with this object and displayed in the UI. */
		TObjectPtr<UMultiUserReplicationSessionPreset> SessionContent;

		/**
		 * The session the local client is in.
		 * 
		 * This FReplicationClientManager's owner is supposed to make sure this FReplicationClientManager is destroyed
		 * when the session shuts down.
		 */
		const TWeakPtr<IConcertClientSession> Session;

		/**
		 * Sends FConcertReplication_QueryReplicationInfo_Request in regular intervals.
		 * Shared by all remote clients so all requests are bundled reducing the number of network requests. 
		 */
		FRegularQueryService QueryService;
		
		/** Manages the local client */
		FLocalReplicationClient LocalClient;
		/** Manages remote clients. Updated when client connects or disconnects to the active session. */
		TArray<FRemoteReplicationClient> RemoteClients;

		/** Called when RemoteClients changes. */
		FRemoteClientsChanged OnRemoteClientsChangedDelegate;

		/** Updates RemoteClients depending on the change. */
		void OnSessionClientChanged(IConcertClientSession&, EConcertClientStatus NewStatus, const FConcertSessionClientInfo& ClientInfo);
		
		/** Shared logic for creating a remote client. */
		void CreateRemoteClient(const FGuid& ClientEndpointId, bool bBroadcastDelegate = true);
	};
}

