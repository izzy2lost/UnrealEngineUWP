// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Replication/IReplicationWorkspace.h"

namespace UE::ConcertSyncTests::Replication
{
	class FReplicationWorkspaceEmptyMock : public ConcertSyncServer::Replication::IReplicationWorkspace
	{
	public:
		
		//~ Begin IReplicationWorkspace Interface
		virtual void ProduceClientLeaveReplicationActivity(const FGuid&, const FConcertSyncReplicationPayload_LeaveReplication&) override {}
		//~ End IReplicationWorkspace Interface
	};
}
