// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "Misc/Optional.h"

struct FConcertReplication_ChangeAuthority_Request;
struct FConcertReplication_ChangeStream_Request;

namespace UE::MultiUserClient
{
	class IClientStreamSynchronizer;
	class FAuthorityChangeTracker;
	class FGlobalAuthorityCache;
	class FStreamChangeTracker;
	
	/**
	 * Util that knows how to build FConcertReplication_ChangeStream_Request and FConcertReplication_ChangeAuthority_Request based on the local client's changes
	 * and knowledge of other remote clients.
	 */
	class FChangeRequestBuilder
	{
	public:

		FChangeRequestBuilder(
			const FGuid& InLocalClientId,
			const FGlobalAuthorityCache& InAuthorityCache,
			IClientStreamSynchronizer& InStreamSynchronizer,
			FStreamChangeTracker& InStreamChangeTracker,
			FAuthorityChangeTracker& AuthorityChangeTracker
			);

		/** @return Valid request that can be sent to the server, if there are any local changes. */
		TOptional<FConcertReplication_ChangeStream_Request> BuildStreamChange() const;

		/** @return Valid request that can be sent to the server, if there are any local changes. */
		TOptional<FConcertReplication_ChangeAuthority_Request> BuildAuthorityChange() const;

	private:

		/** Id of the local client in the session. */
		const FGuid LocalClientId;

		/** Used to filter out changes that would cause a conflict when submitted. */
		const FGlobalAuthorityCache& AuthorityCache;
		
		/** Used to determine whether a new stream needs to be created when submitting (happens if nothing was previously registered). */
		IClientStreamSynchronizer& StreamSynchronizer;
		
		/** Used to get changes made to the stream */
		FStreamChangeTracker& StreamChangeTracker;
		/** Informs us when authority is changed by the user. */
		FAuthorityChangeTracker& AuthorityChangeTracker;

		/** @return The ID MU uses for its single stream. */
		FGuid GetLocalClientStreamId() const;
	};
}

