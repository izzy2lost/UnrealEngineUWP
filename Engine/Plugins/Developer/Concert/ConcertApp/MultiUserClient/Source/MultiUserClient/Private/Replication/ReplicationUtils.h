// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"

#include "Async/Future.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

class IConcertSyncClient;
class IConcertClientReplicationManager;
class SNotificationItem;

namespace UE::MultiUserClient::Replication
{
	struct FJoinSessionResult
	{
		/** Result of joining the replication session. */
		ConcertSyncClient::Replication::FJoinReplicatedSessionResult JoinRequestResult;

		/** Set if an authority request was sent. */
		TOptional<ConcertSyncClient::Replication::FAuthorityChangeResponse> AuthorityResponse;
	};
	
	/**
	 * Decorates a replication join request with additional logic specific to Multi-User workflows.
	 *
	 * Extra steps involve
	 *	- Adds an editor UI notifications on the bottom-right: One for the duration of the process, and then another for
	 *	either success (leaves after a few seconds) or failure (must be dismissed).
	 *	- Automatically takes authority over the objects in the stream (TODO DP ... if it is configured in the multi user settings).
	 *
	 * @note The future can finish on any thread so take care to synchronize if needed!	
	 * @return Future that executes when all actions have been performed (joining, optional authority requests, etc.)
	 */
	TFuture<FJoinSessionResult> JoinSessionForMultiUser(
		const TSharedRef<IConcertSyncClient>& Client,
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args
		);

	/**
	 * Decorates a request for changing authority with additional logic specific to Multi-User workflows.
	 * @note The future can finish on any thread so take care to synchronize if needed!
	 * @return Future that executes when the authority response has been received from the server.
	 */
	TFuture<ConcertSyncClient::Replication::FAuthorityChangeResponse> RequestAuthorityChange(
		IConcertClientReplicationManager& Manager,
		ConcertSyncClient::Replication::FAuthorityChangeRequest Args,
		TSharedPtr<SNotificationItem> NotificationToReuse = nullptr
		);

	/**
	 * Decorates a replication leave requests with additional logic specific to Multi-User workflows.
	 * This shows an editor UI notification on the bottom-right.
	 */
	void LeaveSessionForMultiUser(IConcertClientReplicationManager& Manager);
}
