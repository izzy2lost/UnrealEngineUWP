// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"

enum class EBreakBehavior : uint8;

struct FConcertReplication_ChangeStream_Request;
struct FObjectInStreamID;
struct FObjectReplicationMap;
struct FReplicationStreamDescription;
struct FReplicatedObjectInfo;

/** This namespace contains the shared logic for applying FConcertChangeStream_Requests. */
namespace UE::ConcertSyncCore::Replication::ChangeStreamUtils
{
	/**
	 * Iterates through all objects that are removed and therefore the requesting client would lose authority over.
	 * 
	 * @param Request The request to parse
	 * @param ExistingStreams The streams the request would be applied to
	 * @param Callback The callback to process the objects
	 */
	CONCERTSYNCCORE_API void ForEachObjectLosingAuthority(
		const FConcertReplication_ChangeStream_Request& Request,
		const TArray<FReplicationStreamDescription>& ExistingStreams,
		TFunctionRef<EBreakBehavior(const FObjectInStreamID&)> Callback
		);
	
	/**
	 * Modifies RegisteredStreams as described by the request without validating that the Request is valid to apply.
	 * @param Request The request to parse
	 * @param StreamsToModify The streams to apply the request to
	 */
	CONCERTSYNCCORE_API void ApplyValidatedRequest(
		const FConcertReplication_ChangeStream_Request& Request,
		IN OUT TArray<FReplicationStreamDescription>& StreamsToModify
		);

	/** Iterates entries with incomplete data, which are entries for which IsValidForSendingToServer returns false. */
	CONCERTSYNCCORE_API void IterateInvalidEntries(const FObjectReplicationMap& ReplicationMap, TFunctionRef<EBreakBehavior(const FSoftObjectPath&, const FReplicatedObjectInfo&)> Callback);

	/**
	 * Builds PutObject and RemoveObject requests that would transform Base to Desired.
	 * 
	 * The state of an object in a replication map is only valid if all its members have values:
	 * that means the class is non-null and not the property selection is non-empty.
	 * This function enforces (and ensure()s) that the Base and Desired states are valid.
	 * The changes are validated using FConcertChangeStream_PutObject's factories MakeFromInfo and MakeFromChange.
	 * 
	 * @param StreamId The ID of the changed stream: it is inserted into the request
	 * @param Base The base stream
	 * @param Desired The state the stream should end up in 
	 * @return A semantically correct FConcertChangeStream_Request which Base to Desired (note is can be rejected due to authority conflicts).
	 */
	CONCERTSYNCCORE_API FConcertReplication_ChangeStream_Request BuildRequestFromDiff(
		const FGuid& StreamId,
		const FObjectReplicationMap& Base,
		const FObjectReplicationMap& Desired
		);
}
