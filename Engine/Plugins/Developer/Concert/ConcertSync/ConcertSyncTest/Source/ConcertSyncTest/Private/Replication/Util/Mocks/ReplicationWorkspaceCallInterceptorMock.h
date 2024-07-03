// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IReplicationWorkspace.h"
#include "Replication/Messages/ReplicationActivity.h"

#include "Misc/Optional.h"
#include "Misc/Guid.h"
#include "Templates/Tuple.h"

namespace UE::ConcertSyncTests::Replication
{
	class FReplicationWorkspaceCallInterceptorMock : public ConcertSyncServer::Replication::IReplicationWorkspace
	{
	public:

		/** Arguments of last ProduceClientLeaveReplicationActivity call. */
		TOptional<TTuple<FGuid, FConcertSyncReplicationPayload_LeaveReplication>> LastCall_ProduceClientLeaveReplicationActivity;
		/** Arguments of last ProduceClientMuteReplicationActivity call. */
		TOptional<TTuple<FGuid, FConcertSyncReplicationPayload_Mute>> LastCall_ProduceClientMuteReplicationActivity;
		/** Arguments of last GetLastLeaveReplicationActivityByClient call. */
		mutable TOptional<TTuple<FConcertSessionClientInfo>> LastCall_GetLastLeaveReplicationActivityByClient;
		/** Arguments of last GetLastLeaveReplicationActivityByClient call. */
		mutable TOptional<TTuple<int64>> LastCall_GetLeaveReplicationActivityById;

		/** The result to return in ProduceClientLeaveReplicationActivity. */
		TOptional<int64> ReturnResult_ProduceClientLeaveReplicationActivity = 0;
		/** The result to return in ProduceClientLeaveReplicationActivity. */
		TOptional<int64> ReturnResult_ProduceClientMuteReplicationActivity = 0;
		/** The result to return in GetLastLeaveReplicationActivityByClient. */
		TOptional<FConcertSyncReplicationPayload_LeaveReplication> ReturnResult_GetLastLeaveReplicationActivityByClient;
		/** The result to return in GetLastLeaveReplicationActivityByClient. */
		TOptional<FConcertSyncReplicationPayload_LeaveReplication> ReturnResult_GetLeaveReplicationActivityById;
		
		//~ Begin IReplicationWorkspace Interface
		virtual TOptional<int64> ProduceClientLeaveReplicationActivity(const FGuid& EndpointId, const FConcertSyncReplicationPayload_LeaveReplication& EventData) override
		{
			LastCall_ProduceClientLeaveReplicationActivity = MakeTuple(EndpointId, EventData);
			return ReturnResult_ProduceClientLeaveReplicationActivity;
		}
		virtual TOptional<int64> ProduceClientMuteReplicationActivity(const FGuid& EndpointId, const FConcertSyncReplicationPayload_Mute& EventData) override
		{
			LastCall_ProduceClientMuteReplicationActivity = MakeTuple(EndpointId, EventData);
			return ReturnResult_ProduceClientMuteReplicationActivity;
		}

		virtual bool GetLastLeaveReplicationActivityByClient(const FConcertSessionClientInfo& InClientInfo, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const override
		{
			LastCall_GetLastLeaveReplicationActivityByClient = MakeTuple(InClientInfo);
			if (ReturnResult_GetLastLeaveReplicationActivityByClient)
			{
				OutLeaveReplication = *ReturnResult_GetLastLeaveReplicationActivityByClient;
			}
			return ReturnResult_GetLastLeaveReplicationActivityByClient.IsSet();
		}
		virtual bool GetLeaveReplicationActivityById(const int64 ActivityId, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const override
		{
			LastCall_GetLeaveReplicationActivityById = MakeTuple(ActivityId);
			if (ReturnResult_GetLeaveReplicationActivityById)
			{
				OutLeaveReplication = *ReturnResult_GetLeaveReplicationActivityById;
			}
			return ReturnResult_GetLeaveReplicationActivityById.IsSet();
		}
		//~ End IReplicationWorkspace Interface
	};
}
