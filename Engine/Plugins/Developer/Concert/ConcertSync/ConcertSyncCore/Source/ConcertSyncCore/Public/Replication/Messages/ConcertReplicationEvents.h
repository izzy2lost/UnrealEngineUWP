// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
#include "Replication/Data/ClientQueriedInfo.h"
#include "Replication/Data/ObjectIds.h"
#include "Misc/Optional.h"
#include "ConcertReplicationEvents.generated.h"

class FOutputDevice;

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

/** A request to add a new object to a stream or overwrite a pre-existing object's properties / class. */
USTRUCT()
struct FConcertChangeStream_PutObject
{
	GENERATED_BODY()

	/**
	 * The property selection the object should have.
	 * Objects must always have a non-empty selection: use FConcertChangeStream_Request::ObjectsToRemove to remove objects.
	 *
	 * Any request that would leave an object's property selection empty will result in failure.
	 */
	UPROPERTY()
	FConcertPropertySelection Properties;

	/**
	 * If Object is pre-existing, this property is optional. If different than None, this will change the class
	 * If a new object is added, this property is mandatory. Not doing so will fail the request.
	 *
	 * Not specifying ClassPath nor Properties is an error: use FConcertChangeStream_Request::ObjectsToRemove to remove objects.
	 */
	UPROPERTY()
	FSoftClassPath ClassPath;

	// Intention: Ideally code dealing with PutObject requests uses these constructors / factory functions.
	// If a property is added to FReplicatedObjectInfo, only the below code needs to be updated.

	/** @return The PutObject request if New contained sufficient info - empty otherwise */
	CONCERTSYNCCORE_API static TOptional<FConcertChangeStream_PutObject> MakeFromInfo(const FReplicatedObjectInfo& New);
	/** @return The PutObject request if changing Base to Desired contained sufficient info - empty otherwise */
	CONCERTSYNCCORE_API static TOptional<FConcertChangeStream_PutObject> MakeFromChange(const FReplicatedObjectInfo& Base, const FReplicatedObjectInfo& Desired);

	/** Creates a new object info if there is sufficient data (all fields must be set for this). */
	CONCERTSYNCCORE_API TOptional<FReplicatedObjectInfo> MakeObjectInfoIfValid() const;
};

/**
 * Let's a client change its streams owned on the server.
 * 
 * This request is processed atomically: it either succeeds completely or fails completely.
 * If any sub-change causes a failure, the failure reason will be returned in the response and the client must make a new request.
 */
USTRUCT()
struct FConcertChangeStream_Request
{
	GENERATED_BODY()

	/**
	 * Removes objects from pre-existing streams.
	 * Supplying an object that is not in the specified stream does not cause failure (but is nonsensical).
	 * 
	 * If a stream has no registered objects after this operation, it is automatically removed.
	 * If the requesting client has authority over these objects, authority is removed.
	 */
	UPROPERTY()
	TSet<FObjectInStreamID> ObjectsToRemove;

	/**
	 * Adds new or modifies preexisting object definitions in pre-existing streams.
	 *
	 * If the requesting client and a different client have authority over the same object, you will get a conflict
	 * if the different client already has authority over one of the properties you're adding here.
	 * @see FConcertChangeStream_Response::AuthorityConflicts for some examples of conflicts.
	 *
	 * If the key identifies a stream that does not exist, the request will fail.
	 * 
	 * If the key identifies a stream that is in StreamsToAdd, the request will fail to avoid allowing the construction of
	 * ambiguous requests, e.g. both StreamsToAdd and ObjectsToPut containing object Foo but with different property selections.
	 */
	UPROPERTY()
	TMap<FObjectInStreamID, FConcertChangeStream_PutObject> ObjectsToPut;
	
	/**
	 * New streams to add to the server.
	 * Fails if any ID overlaps with a pre-existing one.
	 *
	 * It is valid to have the same stream in StreamsToRemove: StreamsToAdd is applied after StreamsToRemove resulting
	 * in the stream's content being replaced.
	 */
	UPROPERTY()
	TArray<FReplicationStreamDescription_NetPacked> StreamsToAdd;

	/**
	 * Streams to remove from the server.
	 * Supplying a stream that does not exist does not cause failure (but is nonsensical).
	 * 
	 * If the requesting client has authority over any of the objects contained in the stream, authority is removed.
	 * 
	 * It is valid to have the same stream in StreamsToAdd: StreamsToAdd is applied after StreamsToRemove resulting
	 * in the stream's content being replaced.
	 */
	UPROPERTY()
	TSet<FGuid> StreamsToRemove;
};

UENUM()
enum class EConcertPutObjectErrorCode : uint8
{
	/** Stream that ObjectsToPut referenced was not registered on the server. */
	UnresolvedStream,
	/**
	 * Either PutObject contained no data to update with (ensure either ClassPath or Properties is set),
	 * or it tried to create a new object with insufficient data (make sure ClassPath and Properties are both specified).
	 */
	MissingData
};

/**
 * Contains information about why a request failed. This info could be parsed and displayed to the end user as error.
 * If there is even just one error, the entire requested has failed and no changes were made server-side.
 */
USTRUCT()
struct FConcertChangeStream_Response
{
	GENERATED_BODY()

	/**
	 * Reports dynamic authority errors with ObjectsToPut:
	 * Changing an object over which a client already has authority can yield unresolvable conflicts for which the entire FConcertChangeStream_Request is rejected.
	 *
	 * Let client R be the requester and client A be another client.
	 * Example 1: Overlapping authority on different clients > Conflict
	 * - R has authority over Foo's relative rotation property in stream S.
	 * - A has authority over actor Foo's relative location property in some stream.
	 * - R requests S to include Foo's relative location property: this conflicts with client A's authority.
	 *
	 * Example 2: Overlapping, properties on same client > No conflict
	 * There is no conflict if another stream owned by the requester already has authority over a property you're adding to a different stream.
	 * Streams of the same client may overlap properties. While this may not make much sense and actually be performance degrading, it would result in no logical errors so it is allowed.
	 * Example: R has two streams S1 and S2. S1 contains the transform properties and S2 does not. Both S1 and S2 have authority over object Foo. It is legal to request S2 to contain the transform properties.
	 */
	UPROPERTY()
	TMap<FObjectInStreamID, FReplicatedObjectId> AuthorityConflicts;

	/** Reports semantic errors with ObjectsToPut. */
	UPROPERTY()
	TMap<FObjectInStreamID, EConcertPutObjectErrorCode> ObjectsToPutSemanticErrors;
	
	/** Streams that were in StreamsToAdd but that were not created. */
	UPROPERTY()
	TSet<FGuid> FailedStreamCreation;

	bool IsSuccess() const { return AuthorityConflicts.IsEmpty() && ObjectsToPutSemanticErrors.IsEmpty() && FailedStreamCreation.IsEmpty(); }
	bool IsFailure() const { return !IsSuccess(); }

	/** If IsFailure(), logs the errors. */
	CONCERTSYNCCORE_API void LogErrors(FOutputDevice& OutputDevice) const;
};