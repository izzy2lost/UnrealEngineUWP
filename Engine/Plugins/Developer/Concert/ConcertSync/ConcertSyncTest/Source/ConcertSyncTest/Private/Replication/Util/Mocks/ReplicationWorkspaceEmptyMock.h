// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IReplicationWorkspace.h"

namespace UE::ConcertSyncTests::Replication
{
	class FReplicationWorkspaceEmptyMock : public ConcertSyncServer::Replication::IReplicationWorkspace
	{
	public:
		
		//~ Begin IReplicationWorkspace Interface
		virtual TOptional<int64> ProduceClientLeaveReplicationActivity(const FGuid&, const FConcertSyncReplicationPayload_LeaveReplication&) override { return {}; }
		virtual bool GetLastLeaveReplicationActivityByClient(const FConcertSessionClientInfo& InClientInfo, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const override { return false; }
		virtual bool GetLeaveReplicationActivityById(const int64 ActivityId, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const override { return false; }
		//~ End IReplicationWorkspace Interface
	};
}
