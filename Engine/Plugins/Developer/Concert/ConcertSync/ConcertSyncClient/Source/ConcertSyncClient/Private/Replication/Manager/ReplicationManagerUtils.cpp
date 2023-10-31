// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationManagerUtils.h"

#include "Replication/IConcertClientReplicationManager.h"

#include "Algo/Transform.h"
#include "Containers/Set.h"

namespace UE::ConcertSyncClient::Replication
{
	TFuture<FAuthorityChangeResponse> RejectAll(FAuthorityChangeRequest&& Args)
	{
		return MakeFulfilledPromise<FAuthorityChangeResponse>(
			FAuthorityChangeResponse{{ EReplicationResponseErrorCode::Handled, MoveTemp(Args.TakeAuthority) }}
			).GetFuture();
	}
}
