// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FConcertReplication_ChangeStream_Response;
struct FConcertReplication_ChangeStream_Request;

namespace UE::ConcertSyncServer::Replication
{
	class FAuthorityManager;
	class FConcertReplicationClient;

	/**
	 * Checks whether the stream change is valid to make and puts all errors into OutResponse.
	 * @return Whether the request is valid.
	 */
	bool ValidateStreamChangeRequest(
		const FConcertReplicationClient& Client,
		const FAuthorityManager& AuthorityManager,
		const FConcertReplication_ChangeStream_Request& Request,
		FConcertReplication_ChangeStream_Response& OutResponse
		);
	
	/** @return Whether the request is valid. */
	bool ValidateStreamChangeRequest(
		const FConcertReplicationClient& Client,
		const FAuthorityManager& AuthorityManager,
		const FConcertReplication_ChangeStream_Request& Request
		);
}


