// Copyright Epic Games, Inc. All Rights Reserved.

#include "JoinRequestValidation.h"

#include "Replication/ConcertReplicationClient.h"
#include "Replication/Data/ObjectReplicationMap.h"
#include "Replication/Data/ReplicationStreamDescription.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"

namespace UE::ConcertSyncServer::Replication
{
	static TTuple<EJoinReplicationErrorCode, FString> CheckInputConstraints(
		const FConcertReplication_Join_Request& Request,
		const TMap<FGuid, TSharedRef<FConcertReplicationClient>>& Clients,
		FGetClientName GetClientNameFunc
		)
	{
		/* Duplicate objects and properties might not actually cause any trouble.
		 * However, we will straight out reject any input that is not exactly how we expect it.
		 * This serves us to
		 *		1. reason about our pre/post conditions
		 *		2. since other code can assume said conditions, it is a common security mechanic to reject input in case somebody sends us specifically crafted input
		 */
		TMap<FSoftObjectPath, TArray<const FReplicatedObjectInfo*>> UniqueObjectPropertyDetection;
		TMap<FGuid, FConcertReplicationClient*> UniqueStreamIdDetection;
		TSet<FGuid> UniqueStreamsFromRequestor;

		// Prepare properties and stream IDs already used by other clients 
		for (const TPair<FGuid, TSharedRef<FConcertReplicationClient>>& Client : Clients)
		{
			for (const FReplicationStreamDescription& StreamDescription : Client.Value->GetStreamDescriptions())
			{
				for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& ObjectSelection : StreamDescription.BaseDescription.ReplicationMap.ReplicatedObjects)
				{
					UniqueObjectPropertyDetection.FindOrAdd(ObjectSelection.Key).Add(&ObjectSelection.Value);
				}
				
				const FGuid& ExistingStreamId = StreamDescription.BaseDescription.Identifier;
				check(!UniqueStreamIdDetection.Contains(ExistingStreamId))
				UniqueStreamIdDetection.Add(ExistingStreamId, &Client.Value.Get());
			}
		}
		
		for (const FReplicationStreamDescription_NetPacked& Stream : Request.Streams)
		{
			const FGuid& StreamId = Stream.BaseDescription.Identifier;

			// Another client already using this stream?
			if (FConcertReplicationClient** OwningClient = UniqueStreamIdDetection.Find(StreamId))
			{
				const FString ErrorMessage = FString::Printf(
					TEXT("Stream %s is already in use by client %s"),
					*StreamId.ToString(),
					*GetClientNameFunc((*OwningClient)->GetClientEndpointId())
					);
				return { EJoinReplicationErrorCode::ConflictingStreamId, ErrorMessage };
			}

			// Request contained stream ID twice?
			if (UniqueStreamsFromRequestor.Contains(StreamId))
			{
				return { EJoinReplicationErrorCode::ConflictingStreamId,  FString::Printf(TEXT("Duplicate stream ID %s in request."), *StreamId.ToString()) };
			}
			UniqueStreamsFromRequestor.Add(Stream.BaseDescription.Identifier);

			// Make sure the properties are valid
			for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& ReplicatedObjectInfo : Stream.BaseDescription.ReplicationMap.ReplicatedObjects)
			{
				const FSoftObjectPath& ObjectPath = ReplicatedObjectInfo.Key;
				const FReplicatedObjectInfo& ObjectInfo = ReplicatedObjectInfo.Value;
				
				// Validate class - we cannot validate at this point whether the class really exists.
				if (!ObjectInfo.ClassPath.IsValid())
				{
					const FString ErrorMessage = FString::Printf(
						TEXT("Stream %s has null class for object %s"),
						*Stream.BaseDescription.Identifier.ToString(),
						*ObjectPath.ToString()
						);
					return { EJoinReplicationErrorCode::InvalidClass, ErrorMessage };
				}

				// Validate unique properties
				TSet<FConcertPropertyChain> UniquePropertyDetection;
				const TArray<const FReplicatedObjectInfo*>* PreexistingPropertySelections = UniqueObjectPropertyDetection.Find(ObjectPath);
				for (const FConcertPropertyChain& PropertyChain : ObjectInfo.PropertySelection.ReplicatedProperties)
				{
					// No duplicate properties!
					if (UniquePropertyDetection.Contains(PropertyChain))
					{
						const FString ErrorMessage = FString::Printf(
							TEXT("Stream %s has duplicate property %s for object %s"),
							*Stream.BaseDescription.Identifier.ToString(),
							*PropertyChain.ToString(),
							*ObjectPath.ToString()
						);
						return { EJoinReplicationErrorCode::DuplicateProperty, ErrorMessage };
					}
					UniquePropertyDetection.Add(PropertyChain);

					if (!PreexistingPropertySelections)
					{
						continue;
					}

					// Is another client already streaming these properties?
					for (const FReplicatedObjectInfo* PreexistingObjectSelection : *PreexistingPropertySelections)
					{
						// The request's object class should be the same the class previously assigned to the object in previous requests
						if (PreexistingObjectSelection->ClassPath != ObjectInfo.ClassPath)
						{
							const FString ErrorMessage = FString::Printf(
								TEXT("Class %s in stream %s for object %s does not match expected class %s (another client has told us this object has this class)."),
								*ObjectInfo.ClassPath.ToString(),
								*Stream.BaseDescription.Identifier.ToString(),
								*ObjectPath.ToString(),
								*PreexistingObjectSelection->ClassPath.ToString()
							);
							return { EJoinReplicationErrorCode::InvalidClass, ErrorMessage};
						}

						// Another client (or config in this request) is already using this property
						if (PreexistingObjectSelection->PropertySelection.ReplicatedProperties.Contains(PropertyChain))
						{
							// TODO: In the future we'll change the handshake to no longer reject when another client wants to replicate this property. Adjust the message then.
							// (Instead every client will request authority for each object before replicating)
							const FString ErrorMessage = FString::Printf(
								TEXT("Property %s in stream %s for object %s is already in duplicate or in use by another client"),
								*PropertyChain.ToString(),
								*Stream.BaseDescription.Identifier.ToString(),
								*ObjectPath.ToString()
							);
							return { EJoinReplicationErrorCode::ConflictingAuthority, ErrorMessage };
						}
					}
				}
				
				UniqueObjectPropertyDetection.FindOrAdd(ObjectPath).Add(&ObjectInfo);
			}
		}

		return { EJoinReplicationErrorCode::Success, TEXT("") };
	}
	
	TTuple<EJoinReplicationErrorCode, FString, TArray<FReplicationStreamDescription>> ValidateRequest(
		const FConcertReplication_Join_Request& Request,
		const TMap<FGuid, TSharedRef<FConcertReplicationClient>>& Clients,
		FGetClientName GetClientNameFunc
		)
	{
		if (auto[ErrorCode, ErrorMessage] = CheckInputConstraints(Request, Clients, GetClientNameFunc)
			; ErrorCode != EJoinReplicationErrorCode::Success)
		{
			return { ErrorCode, ErrorMessage, TArray<FReplicationStreamDescription>{} };
		}
		
		TArray<FReplicationStreamDescription> ParsedStreamDescriptions;
		ParsedStreamDescriptions.Reserve(Request.Streams.Num());
		for (const FReplicationStreamDescription_NetPacked& StreamDescription_NetPacked : Request.Streams)
		{
			// The net packed version contains UObject attributes, for which we may not be able to look up the UClass for
			if (TOptional<FReplicationStreamDescription> Description = StreamDescription_NetPacked.Unpack())
			{
				ParsedStreamDescriptions.Emplace(MoveTemp(*Description));
			}
			else
			{
				return {
					EJoinReplicationErrorCode::FailedToUnpackStream,
					FString::Printf(TEXT("Failed to unpack stream %s"), *StreamDescription_NetPacked.BaseDescription.Identifier.ToString()),
					TArray<FReplicationStreamDescription>{}
				};
			}
		}

		return { EJoinReplicationErrorCode::Success, TEXT(""), ParsedStreamDescriptions };
	}
}
