// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IReplicationWorkspace.h"
#include "Replication/Messages/ReplicationActivity.h"

#include "Misc/Optional.h"
#include "Misc/Guid.h"
#include "Templates/Tuple.h"

namespace UE::ConcertSyncTests::Replication
{
	class FReplicationWorkspaceCallDetectorMock : public ConcertSyncServer::Replication::IReplicationWorkspace
	{
	public:

		/** Arguments of last ProduceClientLeaveReplicationActivity call. */
		TOptional<TTuple<FGuid, FConcertSyncReplicationPayload_LeaveReplication>> LastCall_ProduceClientLeaveReplicationActivity;
		
		//~ Begin IReplicationWorkspace Interface
		virtual void ProduceClientLeaveReplicationActivity(const FGuid& EndpointId, const FConcertSyncReplicationPayload_LeaveReplication& EventData) override
		{
			LastCall_ProduceClientLeaveReplicationActivity = MakeTuple(EndpointId, EventData);
		}
		//~ End IReplicationWorkspace Interface
	};
}
