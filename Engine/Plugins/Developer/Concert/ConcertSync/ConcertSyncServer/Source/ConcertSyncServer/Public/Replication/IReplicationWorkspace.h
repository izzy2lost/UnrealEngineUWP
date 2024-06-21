// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FConcertSyncReplicationPayload_LeaveReplication;
struct FGuid;

namespace UE::ConcertSyncServer::Replication
{
	/**
	 * Interface that FConcertServerReplicationManager uses to interact with the FConcertServerWorkspace.
	 * Allows mocking in unit tests, which is the only reason it's in the public module interface.
	 */
	class IReplicationWorkspace
	{
	public:
		
		/** Creates a replication activity for the client leaving replication if the session has the EConcertSyncSessionFlags::ShouldEnableReplicationActivities flag. */
		virtual void ProduceClientLeaveReplicationActivity(const FGuid& EndpointId, const FConcertSyncReplicationPayload_LeaveReplication& EventData) = 0;

		virtual ~IReplicationWorkspace() = default;
	};
}
