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

	bool FAuthorityManager::HasAuthorityToChange(const FReplicatedObjectId& ObjectChange) const
	{
		const FClientAuthorityData* AuthorityData = ClientAuthorityData.Find(ObjectChange.SenderEndpointId);
		const TSet<FSoftObjectPath>* OwnedObjects = AuthorityData ? AuthorityData->OwnedObjects.Find(ObjectChange.StreamId) : nullptr;
		return OwnedObjects && OwnedObjects->Contains(ObjectChange.Object);
	}

	void FAuthorityManager::EnumerateAuthority(const FClientId& ClientId, const FStreamId& StreamId, TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Callback) const
	{
		const FClientAuthorityData* AuthorityData = ClientAuthorityData.Find(ClientId);
		const TSet<FSoftObjectPath>* OwnedObjects = AuthorityData ? AuthorityData->OwnedObjects.Find(StreamId) : nullptr;
		if (!OwnedObjects)
		{
			return;
		}
		
		for (const FSoftObjectPath& AuthoredObject : *OwnedObjects)
		{
			if (Callback(AuthoredObject) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	FAuthorityManager::EAuthorityResult FAuthorityManager::EnumerateAuthorityConflicts(
		const FReplicatedObjectId& Object, 
		const FConcertPropertySelection* OverwriteProperties,
		FProcessAuthorityConflict ProcessConflict
		) const
	{
		const FClientId& ClientId = Object.SenderEndpointId;
		const FConcertPropertySelection* PropertiesToCheck = OverwriteProperties;
		if (!PropertiesToCheck)
		{
			const FReplicationStreamDescription* Description = FindClientStreamById(ClientId, Object.StreamId);
			const FReplicatedObjectInfo* PropertyInfo = Description ? Description->BaseDescription.ReplicationMap.ReplicatedObjects.Find(Object.Object) : nullptr;
			PropertiesToCheck = PropertyInfo ? &PropertyInfo->PropertySelection : nullptr;
		}

		if (!PropertiesToCheck)
		{
			return EAuthorityResult::NoRegisteredProperties;
		}

		bool bFreeOfPropertyOverlaps = true;
		const FClientId IgnoredClients[] = { ClientId };
		ForEachClientWithPotentialConflict(
			Object.Object,
			[this, &ProcessConflict, PropertiesToCheck, &bFreeOfPropertyOverlaps](const FClientId& ClientId, const FStreamId& StreamId, const FConcertPropertySelection& WrittenProperties) mutable
			{
				// At this point we know that WritingClientId is sending WrittenProperties - if the properties overlap, the authority request is not possible
				const bool bHasNoConflict = !WrittenProperties.OverlapsWith(*PropertiesToCheck);
				bFreeOfPropertyOverlaps &= bHasNoConflict;
				return bHasNoConflict
					? EBreakBehavior::Continue
					: ProcessConflict(ClientId, StreamId, WrittenProperties);
			},
			IgnoredClients
			);
		return bFreeOfPropertyOverlaps ? EAuthorityResult::Allowed : EAuthorityResult::Conflict;
	}

	bool FAuthorityManager::CanTakeAuthority(const FReplicatedObjectId& Object) const
	{
		return EnumerateAuthorityConflicts(Object) == EAuthorityResult::Allowed;
	}

	void FAuthorityManager::OnClientLeft(const FClientId& ClientEndpointId)
	{
		ClientAuthorityData.Remove(ClientEndpointId);
	}

	void FAuthorityManager::RemoveAuthority(const FReplicatedObjectId& Object)
	{
		const FClientId& ClientId = Object.SenderEndpointId;
		FClientAuthorityData* ClientData = ClientAuthorityData.Find(ClientId);
		if (!ClientData)
		{
			return;
		}

		const FStreamId& StreamId = Object.StreamId;
		TSet<FSoftObjectPath>* AuthoredObjects = ClientData->OwnedObjects.Find(StreamId);
		if (!AuthoredObjects)
		{
			return;
		}

		// This is all that is needed to remove authority.
		AuthoredObjects->Remove(Object.Object);
		
		// Clean-up potentially empty entries
		if (AuthoredObjects->IsEmpty())
		{
			ClientAuthorityData.Remove(StreamId);
			if (ClientData->OwnedObjects.IsEmpty())
			{
				ClientAuthorityData.Remove(ClientId);
			}
		}
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
			const FReplicatedObjectId ObjectToAuthor{ { StreamId, ObjectPath }, ClientId };
			if (CanTakeAuthority(ObjectToAuthor))
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
			const FReplicatedObjectId ObjectToAuthor{ { StreamId, ObjectPath }, ClientId };
			if (CanTakeAuthority(ObjectToAuthor))
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
		
		Private::ForEachReplicatedObject(Request.ReleaseAuthority, [this, &AuthorityData](const FStreamId& StreamId, const FSoftObjectPath& ObjectPath)
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
		FProcessAuthorityConflict Callback,
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
				const FStreamId& StreamId = Stream.BaseDescription.Identifier;
				const TSet<FSoftObjectPath>* ClientControlledObjects = OtherClientData->OwnedObjects.Find(StreamId);
				const bool bClientHasSomeAuthorityOverObject = ClientControlledObjects && ClientControlledObjects->Contains(Object);
				if (!bClientHasSomeAuthorityOverObject)
				{
					return EBreakBehavior::Continue;
				}

				// This client is using the request object: report the potential conflict ...
				const FObjectReplicationMap& ObjectReplicationMap = Stream.BaseDescription.ReplicationMap;
				const FReplicatedObjectInfo* ReplicationObjectInfo = ObjectReplicationMap.ReplicatedObjects.Find(Object);
				if (ReplicationObjectInfo && Callback(ClientEndpointId, StreamId, ReplicationObjectInfo->PropertySelection) == EBreakBehavior::Break)
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
