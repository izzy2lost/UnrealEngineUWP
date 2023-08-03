// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
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
	FGuid StreamId;

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
 */
USTRUCT()
struct FConcertBatchReplicationEvent
{
	GENERATED_BODY()
	
	/** The objects replicated by this event */
	UPROPERTY()
	TArray<FConcertStreamReplicationEvent> Streams;
};