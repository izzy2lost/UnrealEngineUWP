// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/ChangeOperationTypes.h"
#include "Replication/Messages/ChangeAuthority.h"
#include "Replication/Messages/ChangeStream.h"

#include "Misc/Optional.h"

struct FConcertObjectReplicationMap;
struct FGuid;

namespace UE::MultiUserClient
{
	enum class EChangeStreamOperationResult : uint8;
	enum class EChangeAuthorityOperationResult : uint8;
	
	struct FSubmitAuthorityChangesResponse;
	struct FSubmitStreamChangesResponse;
}

/**
 * Utils for transforming the simplified params in ChangeOperationTypes.h to the ones used by ConcertSyncCore.
 */
namespace UE::MultiUserClient::ClientChangeConversionUtils
{
	/** Transforms FChangeStreamRequest to FConcertReplication_ChangeStream_Request, if the request is correctly formatted. */
	TOptional<FConcertReplication_ChangeStream_Request> Transform(
		FChangeStreamRequest Request,
		const FGuid& ClientStreamId,
		const FConcertObjectReplicationMap& ClientStreamContent
		);
	
	/** Transforms FChangeAuthorityRequest to FConcertReplication_ChangeStream_Request, if the request is correctly formatted. */
	TOptional<FConcertReplication_ChangeAuthority_Request> Transform(
		FChangeAuthorityRequest Request,
		const FGuid& ClientStreamId
		);


	/** Extract an error code from FSubmitStreamChangesResponse. */
	EChangeStreamOperationResult Transform(const FSubmitStreamChangesResponse& Response);
	/** Extract an error code from FSubmitAuthorityChangesResponse. */
	EChangeAuthorityOperationResult Transform(const FSubmitAuthorityChangesResponse& Response);


	inline TOptional<FConcertReplication_ChangeStream_Request> Transform(
		TOptional<FChangeStreamRequest> Request,
		const FGuid& ClientStreamId,
		const FConcertObjectReplicationMap& ClientStreamContent
		)
	{
		return Request
			? Transform(MoveTemp(*Request), ClientStreamId, ClientStreamContent)
			: TOptional<FConcertReplication_ChangeStream_Request>{};
	}
	
	inline TOptional<FConcertReplication_ChangeAuthority_Request> Transform(
		TOptional<FChangeAuthorityRequest> Request,
		const FGuid& ClientStreamId
		)
	{
		return Request
			? Transform(MoveTemp(*Request), ClientStreamId)
			: TOptional<FConcertReplication_ChangeAuthority_Request>{};
	}
}

