// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuthorityManager.h"

#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/Data/ObjectIds.h"
#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/Messages/ConcertReplicationEvents.h"

namespace UE::ConcertSyncServer::Replication
{
	namespace Private
	{
		static void ForEachReplicatedObject(
			const TMap<FSoftObjectPath, FConcertStreamArray>& Map,
			TFunctionRef<void(const FGuid& StreamId, const FSoftObjectPath& ObjectPath)> Callback
			)
		{
			for (const TPair<FSoftObjectPath, FConcertStreamArray>& ObjectInfo : Map)
			{
				for (const FGuid& StreamId : ObjectInfo.Value.StreamIds)
				{
					Callback(StreamId, ObjectInfo.Key);
				}
			}
		}
	}
	
	FAuthorityManager::FAuthorityManager(IAuthorityManagerGetters& Getters, TSharedRef<IConcertSession> InSession)
		: Getters(Getters)
		, Session(MoveTemp(InSession))
	{
		Session->RegisterCustomRequestHandler<FConcertChangeAuthority_Request, FConcertChangeAuthority_Response>(this, &FAuthorityManager::HandleChangeAuthorityRequest);
	}

	FAuthorityManager::~FAuthorityManager()
	{
		Session->UnregisterCustomRequestHandler<FConcertChangeAuthority_Request>();
	}

	bool FAuthorityManager::IsObjectChangeAllowed(const FReplicatedObjectId& ObjectChange) const
	{
		const FClientAuthorityData* AuthorityData = ClientAuthorityData.Find(ObjectChange.SenderEndpointId);
		const TSet<FSoftObjectPath>* OwnedObjects = AuthorityData ? AuthorityData->OwnedObjects.Find(ObjectChange.StreamId) : nullptr;
		return OwnedObjects && OwnedObjects->Contains(ObjectChange.Object);
	}

	void FAuthorityManager::OnClientLeft(const FClientId& ClientEndpointId)
	{
		ClientAuthorityData.Remove(ClientEndpointId);
	}

	EConcertSessionResponseCode FAuthorityManager::HandleChangeAuthorityRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertChangeAuthority_Request& Request,
		FConcertChangeAuthority_Response& Response
		)
	{
		// This log does two things: 1. Identify issues in unit tests / at runtime 2. Warn about possibly malicious attempts when the server runs.
		UE_CLOG(Request.TakeAuthority.IsEmpty() && Request.ReleaseAuthority.IsEmpty(), LogConcert, Warning, TEXT("Received invalid authority request (TakeAuthority.Num() == 0 and ReleaseAuthority.Num() == 0)"));
		
		FClientAuthorityData& AuthorityData = ClientAuthorityData.FindOrAdd(ConcertSessionContext.SourceEndpointId);
		const FClientId& ClientId = ConcertSessionContext.SourceEndpointId;
		Private::ForEachReplicatedObject(Request.TakeAuthority, [this, &Response, &AuthorityData, &ClientId](const FStreamId& StreamId, const FSoftObjectPath& ObjectPath)
		{
			if (CanTakeAuthority(AuthorityData, ClientId, { StreamId, ObjectPath }))
			{
				UE_LOG(LogConcert, Log, TEXT("Transferred authority of %s to client %s for their stream %s"), *ObjectPath.ToString(), *ClientId.ToString(EGuidFormats::Short), *StreamId.ToString(EGuidFormats::Short));
				AuthorityData.OwnedObjects.FindOrAdd(StreamId).Add(ObjectPath);
			}
			else
			{
				UE_LOG(LogConcert, Log, TEXT("Rejected %s request of authority over %s in stream %s"), *ClientId.ToString(EGuidFormats::Short), *ObjectPath.ToString(), *StreamId.ToString(EGuidFormats::Short));
				Response.RejectedObjects.FindOrAdd(ObjectPath).StreamIds.Add(ClientId);
			}
		});
		
		Private::ForEachReplicatedObject(Request.TakeAuthority, [this, &Response, &AuthorityData, &ClientId](const FStreamId& StreamId, const FSoftObjectPath& ObjectPath)
		{
			if (CanTakeAuthority(AuthorityData, ClientId, { StreamId, ObjectPath }))
			{
				UE_LOG(LogConcert, Log, TEXT("Transferred authority of %s to client %s for their stream %s"), *ObjectPath.ToString(), *ClientId.ToString(EGuidFormats::Short), *StreamId.ToString(EGuidFormats::Short));
				AuthorityData.OwnedObjects.FindOrAdd(StreamId).Add(ObjectPath);
			}
			else
			{
				UE_LOG(LogConcert, Log, TEXT("Rejected %s request of authority over %s in stream %s"), *ClientId.ToString(EGuidFormats::Short), *ObjectPath.ToString(), *StreamId.ToString(EGuidFormats::Short));
				Response.RejectedObjects.Add(ObjectPath);
			}
		});
		
		Private::ForEachReplicatedObject(Request.ReleaseAuthority, [this, &Response, &AuthorityData, &ClientId](const FStreamId& StreamId, const FSoftObjectPath& ObjectPath)
		{
			TSet<FSoftObjectPath>* OwnedObjects = AuthorityData.OwnedObjects.Find(StreamId);
			// Though dubious, it is a valid request for the client to release non-owned objects
			if (!OwnedObjects)
			{
				return;
			}

			OwnedObjects->Remove(ObjectPath);
			// Avoid memory leaking
			if (OwnedObjects->IsEmpty())
			{
				AuthorityData.OwnedObjects.Remove(StreamId);
			}
		});

		return EConcertSessionResponseCode::Success;
	}

	bool FAuthorityManager::CanTakeAuthority(const FClientAuthorityData& ClientData, const FClientId& ClientId, const FObjectInStreamID& Object) const
	{
		/*
		 * This function is unoptimized: it repeatedly iterates through every client's registered streams and properties.
		 * However it should be fine since:
		 * 1. Changing authority does not happen too often though,
		 * 2. There will be a relatively small number of clients, streams, objects & properties.
		 */
		
		const FReplicationStreamDescription* Description = FindClientStreamById(ClientId, Object.StreamId);
		const FSoftObjectPath& ObjectPath = Object.Object;
		const FReplicatedObjectInfo* PropertyInfo = Description ? Description->BaseDescription.ReplicationMap.ReplicatedObjects.Find(ObjectPath) : nullptr;
		if (!PropertyInfo)
		{
			return false;
		}

		bool bFreeOfPropertyOverlaps = true;
		const FClientId IgnoredClients[] = { ClientId };
		ForEachClientWithPotentialConflict(ObjectPath, [this, &ClientId, &PropertyInfo, &bFreeOfPropertyOverlaps](const FClientId& WritingClientId, const FConcertPropertySelection& WrittenProperties) mutable
		{
			// At this point we know that WritingClientId is sending WrittenProperties - if the properties overlap, the authority request is not possible
			bFreeOfPropertyOverlaps &= !WrittenProperties.OverlapsWith(PropertyInfo->PropertySelection);
			if (bFreeOfPropertyOverlaps)
			{
				return EBreakBehavior::Continue;
			}
			return EBreakBehavior::Break;
		}, IgnoredClients);
		return bFreeOfPropertyOverlaps;
	}
	
	const FReplicationStreamDescription* FAuthorityManager::FindClientStreamById(const FClientId& ClientId, const FStreamId& StreamId) const
	{
		const FReplicationStreamDescription* StreamDescription = nullptr;
		Getters.ForEachStream(ClientId, [&StreamId, &StreamDescription](const FReplicationStreamDescription& Stream) mutable
		{
			if (Stream.BaseDescription.Identifier == StreamId)
			{
				StreamDescription = &Stream;
				return EBreakBehavior::Break;
			}
			return EBreakBehavior::Continue;
		});
		return StreamDescription;
	}

	void FAuthorityManager::ForEachClientWithPotentialConflict(
		const FSoftObjectPath& Object,
		TFunctionRef<EBreakBehavior(const FClientId& ClientId, const FConcertPropertySelection& WrittenProperties)> Callback,
		TArrayView<const FClientId> IgnoredClients
		) const
	{
		Getters.ForEachSendingClient([this, &Object, &Callback, &IgnoredClients](const FGuid& ClientEndpointId)
		{
			if (IgnoredClients.Contains(ClientEndpointId))
			{
				return EBreakBehavior::Continue;
			}

			// If client has no authority at all, skip.
			const FClientAuthorityData* OtherClientData = ClientAuthorityData.Find(ClientEndpointId);
			if (!OtherClientData)
			{
				return EBreakBehavior::Continue;
			}
			
			EBreakBehavior Result = EBreakBehavior::Continue;
			Getters.ForEachStream(ClientEndpointId, [&Object, &Callback, &ClientEndpointId, OtherClientData, &Result](const FReplicationStreamDescription& Stream) mutable
			{
				// If client has not claimed authority over this object in this stream, skip
				const FGuid& StreamId = Stream.BaseDescription.Identifier;
				const TSet<FSoftObjectPath>* ClientControlledObjects = OtherClientData->OwnedObjects.Find(StreamId);
				const bool bClientHasSomeAuthorityOverObject = ClientControlledObjects && ClientControlledObjects->Contains(Object);
				if (!bClientHasSomeAuthorityOverObject)
				{
					return EBreakBehavior::Continue;
				}

				// This client is using the request object: report the potential conflict ...
				const FObjectReplicationMap& ObjectReplicationMap = Stream.BaseDescription.ReplicationMap;
				const FReplicatedObjectInfo* ReplicationObjectInfo = ObjectReplicationMap.ReplicatedObjects.Find(Object);
				if (ReplicationObjectInfo && Callback(ClientEndpointId, ReplicationObjectInfo->PropertySelection) == EBreakBehavior::Break)
				{
					// ... conflict ends iteration
					Result = EBreakBehavior::Break;
					return EBreakBehavior::Break;
				}
				// ... conflict resolved
				return EBreakBehavior::Continue;
			});
			return Result;
		});
	}
}
