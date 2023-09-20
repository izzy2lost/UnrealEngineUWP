// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertServerReplicationManager.h"

#include "AuthorityManager.h"
#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/ChangeStreamSharedUtils.h"
#include "Replication/ConcertReplicationClient.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/Processing/ObjectReplicationCache.h"

namespace UE::ConcertSyncServer::Replication
{
	namespace Private
	{
		static const FReplicationStreamDescription* FindExistingStream(const FConcertReplicationClient& Client, const FGuid& StreamId)
		{
			return Client.GetStreamDescriptions().FindByPredicate([&StreamId](const FReplicationStreamDescription& Description)
			{
				return Description.BaseDescription.Identifier == StreamId;
			});
		}
		
		/** Validates that ObjectsToPut writes only to pre-existing streams. */
		static void ValidatePutObjectsRequestSemantics(const FConcertChangeStream_Request& Request, const FConcertReplicationClient& Client, FConcertChangeStream_Response& OutResponse)
		{
			const auto PutObjectHasEnoughData = [](const FReplicationStreamDescription* ExistingStream, const FSoftObjectPath& ChangedObjectPath, const FConcertChangeStream_PutObject& PutObject)
			{
				const bool bHasProperties = !PutObject.Properties.ReplicatedProperties.IsEmpty();
				const bool bHasClassPath = !PutObject.ClassPath.IsNull();
				const bool bIsEditingExistingObjectDefinition = ExistingStream->BaseDescription.ReplicationMap.ReplicatedObjects.Contains(ChangedObjectPath);
				const bool bHasEnoughData = (!bIsEditingExistingObjectDefinition && bHasProperties && bHasClassPath)
					|| (bIsEditingExistingObjectDefinition && bHasProperties)
					|| (bIsEditingExistingObjectDefinition && bHasClassPath);
				return bHasEnoughData;
			};
			
			for (const TPair<FObjectInStreamID, FConcertChangeStream_PutObject>& Change : Request.ObjectsToPut)
			{
				const FObjectInStreamID ChangedObject = Change.Key;
				
				const FGuid& StreamToModify = ChangedObject.StreamId;
				const FReplicationStreamDescription* ExistingStream = FindExistingStream(Client, StreamToModify);
				const bool bStreamExists = ExistingStream != nullptr;
				if (bStreamExists)
				{
					// FReplicatedObjectInfo must have non-empty values for its members. Hence we must check that
					// 1. if creating a new entry, both fields are given
					// 2. if writing to pre-existing entry, at least one field is given
					const FConcertChangeStream_PutObject& PutObject = Change.Value; 
					if (!PutObjectHasEnoughData(ExistingStream, ChangedObject.Object, PutObject))
					{
						OutResponse.ObjectsToPutSemanticErrors.Add(ChangedObject, EConcertPutObjectErrorCode::MissingData);
					}
				}
				else
				{
					// ObjectsToPut can only write to pre-existing streams
					OutResponse.ObjectsToPutSemanticErrors.Add(ChangedObject, EConcertPutObjectErrorCode::UnresolvedStream);
				}
			}
		}

		/** Checks that StreamsToAdd do not conflict with pre-existing ones and that all IDs in the request are also unique. */
		static void ValidateAddedStreamsAreUnique(const FConcertChangeStream_Request& Request, const FConcertReplicationClient& Client, FConcertChangeStream_Response& OutResponse)
		{
			TSet<FGuid> DuplicateEntryDetection;
			for (const FReplicationStreamDescription_NetPacked& NewStream : Request.StreamsToAdd)
			{
				// StreamsToAdd is invalid if there is already a stream with the same ID registered ...
				const FGuid& NewStreamId = NewStream.BaseDescription.Identifier;
				const bool bIdAlreadyExists = FindExistingStream(Client, NewStreamId) != nullptr;

				// ... or StreamsToAdd contains the same ID multiple times
				bool bIsDuplicateEntry = false;
				DuplicateEntryDetection.FindOrAdd(NewStreamId, &bIsDuplicateEntry);
				
				if (bIdAlreadyExists || bIsDuplicateEntry)
				{
					OutResponse.FailedStreamCreation.Add(NewStreamId);
				}
			}
		}

		/** If the requesting client has authority over a changed object, checks that no other client has authority over the properties being added. */
		static void LookForAuthorityConflicts(const FConcertChangeStream_Request& Request, const FConcertReplicationClient& Client, const FAuthorityManager& AuthorityManager, FConcertChangeStream_Response& OutResponse)
		{
			const FGuid ClientEndpointId = Client.GetClientEndpointId();
			for (const TPair<FObjectInStreamID, FConcertChangeStream_PutObject>& PutObjectPair : Request.ObjectsToPut)
			{
				// No conflict possible if requesting client does not have authority over the changed object
				const FReplicatedObjectId ReplicatedObjectInfo { { PutObjectPair.Key }, ClientEndpointId };
				if (!AuthorityManager.HasAuthorityToChange(ReplicatedObjectInfo))
				{
					continue;
				}

				// Requester not changing properties (must be changing ClassPath)? Also no conflict possible.
				const FConcertPropertySelection& PropertySelection = PutObjectPair.Value.Properties;
				const bool bIsEditingProperties = !PropertySelection.ReplicatedProperties.IsEmpty();
				if (!bIsEditingProperties)
				{
					continue;
				}

				// Simply check whether any other client is already sending any of the requested properties.
				AuthorityManager.EnumerateAuthorityConflicts(ReplicatedObjectInfo, &PropertySelection,
					[&OutResponse, &ReplicatedObjectInfo](const FGuid& ClientId, const FGuid& StreamId, const FConcertPropertySelection& WrittenProperties)
					{
						const FReplicatedObjectId ConflictingObject = { { StreamId, ReplicatedObjectInfo.Object }, ClientId };
						OutResponse.AuthorityConflicts.Add(ReplicatedObjectInfo, ConflictingObject);
						return EBreakBehavior::Continue;
					});
			}
		}

		/** Stream description can have UObjects attached which fail to serialize on the receiving end. If so, reject. */
		static void ValidateUnpacking(const FConcertChangeStream_Request& Request, FConcertChangeStream_Response& OutResponse)
		{
			for (const FReplicationStreamDescription_NetPacked& ToUnpack : Request.StreamsToAdd)
			{
				if (!ToUnpack.Unpack())
				{
					OutResponse.FailedStreamCreation.Add(ToUnpack.BaseDescription.Identifier);
				}
			}
		}

		/** Checks whether this request is valid to apply. */
		static bool ShouldAcceptRequest(const FConcertChangeStream_Request& Request, const FConcertReplicationClient& Client, const FAuthorityManager& AuthorityManager, FConcertChangeStream_Response& OutResponse)
		{
			ValidatePutObjectsRequestSemantics(Request, Client, OutResponse);
			ValidateAddedStreamsAreUnique(Request, Client, OutResponse);
			LookForAuthorityConflicts(Request, Client, AuthorityManager, OutResponse);
			ValidateUnpacking(Request, OutResponse);
			return OutResponse.IsSuccess();
		}
	}

	EConcertSessionResponseCode FConcertServerReplicationManager::HandleChangeStreamRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertChangeStream_Request& Request,
		FConcertChangeStream_Response& Response)
	{
		const FGuid SendingClientId = ConcertSessionContext.SourceEndpointId;
		const TSharedRef<FConcertReplicationClient>* SendingClient = Clients.Find(SendingClientId);
		if (SendingClient && Private::ShouldAcceptRequest(Request, SendingClient->Get(), AuthorityManager.Get(), Response))
		{
			// If the client had authority over any objects that were removed by this request, authority must be cleaned up
			ConcertSyncCore::Replication::ChangeStreamUtils::ForEachObjectLosingAuthority(Request, SendingClient->Get().GetStreamDescriptions(),
				[this, &SendingClientId](const FObjectInStreamID& RemovedObject)
				{
					AuthorityManager->RemoveAuthority({ RemovedObject, SendingClientId});
					return EBreakBehavior::Continue;
				});
			
			SendingClient->Get().ApplyValidatedRequest(Request);
		}
		else
		{
			UE_LOG(LogConcert, Warning, TEXT("Rejecting ChangeStream request from %s"), *SendingClientId.ToString(EGuidFormats::Short));
		}
		
		return EConcertSessionResponseCode::Success;
	}
}

