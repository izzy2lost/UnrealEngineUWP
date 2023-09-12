// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
#include "Replication/Data/ClientQueriedInfo.h"
#include "ConcertReplicationEvents.generated.h"

/** Contains data to be applied to a replicated object. */
USTRUCT()
struct FConcertObjectReplicationEvent
{
	GENERATED_BODY()

	/**
	 * The object to serialize into.
	 * TODO: To reduce network load, replace this with an int32 that maps into FObjectToIdTable (custom) / UPackageMap (engine)  shared by all clients and the server.
	 */
	UPROPERTY()
	FSoftObjectPath ReplicatedObject;
	
	/** Contains another struct as payload, such as FConcertFullObjectReplicationData. */
	UPROPERTY()
	FConcertSessionSerializedPayload SerializedPayload;
};

/** Contains data produced by a specific replication stream. */
USTRUCT()
struct FConcertStreamReplicationEvent
{
	GENERATED_BODY()

	/** The stream that produced this data. */
	UPROPERTY()
	FGuid StreamId; // TODO DP UE-193261: This field is only relevant to the server and useless for receiving clients.

	/** The objects replicated by this event */
	UPROPERTY()
	TArray<FConcertObjectReplicationEvent> ReplicatedObjects;

	FConcertStreamReplicationEvent() = default;
	explicit FConcertStreamReplicationEvent(const FGuid& StreamId)
		: StreamId(StreamId)
	{}
};

/**
 * Contains multiple objects to be replicated.
 * Sent from
 *  - Authoritative client to server
 *  - Server to clients. Every event may be customized for each client (clients can choose to ignore object updates).
 *
 *  TODO DP UE-193261: This should be split up into FConcertBatchReplicationEvent_ToServer and FConcertBatchReplicationEvent_ToClient; clients do not need FConcertStreamReplicationEvent::StreamId. 
 */
USTRUCT()
struct FConcertBatchReplicationEvent 
{
	GENERATED_BODY()
	
	/** The objects replicated by this event */
	UPROPERTY()
	TArray<FConcertStreamReplicationEvent> Streams;
};

USTRUCT()
struct FConcertStreamArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGuid> StreamIds;
};

/**
 * Sent by clients to the server to attempt to take or release authority over properties on objects.
 * Clients must have authority over object properties before sending FConcertBatchReplicationEvent, or the server will reject the updates.
 * 
 * Multiple clients can have authority over the same objects as long as the properties do not overlap.
 * The properties are defined implicitly by providing the stream ID that will be writing the properties.
 */
USTRUCT()
struct FConcertChangeAuthority_Request
{
	GENERATED_BODY()

	// In the future we can consider adding an option to make this all or nothing (if one object fails, the entire request does)
	// It is supposed to be a convenience and network optimization to send one big request instead of many small ones.

	/**
	 * Objects the client requests to take authority over.
	 * Mapping of object to stream identifiers the client has previously registered.
	 */
	UPROPERTY()
	TMap<FSoftObjectPath, FConcertStreamArray> TakeAuthority;

	/**
	 * Objects the client no longer needs authority over.
	 * Mapping of object to stream identifiers the client has previously registered.
	 * 
	 * The request will not fail if it contains objects it has no authority over.
	 */
	UPROPERTY()
	TMap<FSoftObjectPath, FConcertStreamArray> ReleaseAuthority;
};

USTRUCT()
struct FConcertChangeAuthority_Response
{
	GENERATED_BODY()

	/**
	 * Objects streams the client did not receive authority over.
	 * The client is implied to have authority over all other request objects.
	 */
	UPROPERTY()
	TMap<FSoftObjectPath, FConcertStreamArray> RejectedObjects;
};

/** Flags to affect FConcertQueryClientStreams_Request */
UENUM()
enum class EConcertQueryClientStreamFlags : uint8
{
	None = 0,

	/**
	 * If set, FReplicationClientQueriedInfo::Streams will be empty.
	 */
	SkipStreamInfo = 1 << 0,

	/**
	 * Optimization. The returned streams will not contain the properties that are sent.
	 * Set this if the request only cares for the objects being replicated but not its content.
	 * Only has an effect if SkipStreamInfo is not set.
	 */
	SkipProperties = 1 << 1,
	
	/**
	 * If set, FReplicationClientQueriedInfo::Authority will be empty.
	 */
	SkipAuthority = 1 << 2
};
ENUM_CLASS_FLAGS(EConcertQueryClientStreamFlags)

/**
 * Let's a client query about another client's registered streams.
 *
 * Currently, clients must continuously poll to detect when a client changes this data.
 * TODO: Add an event with which clients can be notified when another client changes their profile.
 * TODO: Alternative to an event, we can add a version int which is incremented every time the info changes.
 */
USTRUCT()
struct FConcertQueryReplicationInfo_Request
{
	GENERATED_BODY()

	/** The clients to query about */
	UPROPERTY()
	TSet<FGuid> ClientEndpointIds;

	UPROPERTY()
	EConcertQueryClientStreamFlags QueryFlags = EConcertQueryClientStreamFlags::None;
};

USTRUCT()
struct FConcertQueryReplicationInfo_Response
{
	GENERATED_BODY()

	/**
	 * Binds client participating in the replication session to their info.
	 * 
	 * @note This contains only the client endpoint IDs that are in the replication session at the time of processing the request.
	 * That means not every endpoint ID that was in the request's ClientEndpointIds will necessarily be in this result. For example,
	 * if client A sends a FConcertQueryReplicationInfo_Request while client B disconnects, client B may not show up in ClientInfo.
	 * 
	 * Key: Client endpoint ID.
	 * Value: Info about the client.
	 */
	UPROPERTY()
	TMap<FGuid, FReplicationClientQueriedInfo> ClientInfo;
};