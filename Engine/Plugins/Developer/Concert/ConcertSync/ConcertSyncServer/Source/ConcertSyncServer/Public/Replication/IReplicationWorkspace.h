// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

struct FConcertClientInfo;
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

		/**
		 * Gets the last replication leave activity associated for a given client.
		 * 
		 * As endpoint IDs change every time a client join a session, the look up is done by client display name.
		 * If multiple machines joined with the same display name, the tie is broken by also using the device name.
		 * 
		 * @param InClientInfo Info about the client for which to get the activity.
		 * @param OutLeaveReplication The activity, if present
		 * @return Whether OutLeaveReplication contains a valid result.
		 */
		virtual bool GetLastLeaveReplicationActivityByClient(const FConcertClientInfo& InClientInfo, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const = 0;
		/** Gets the replication leave activity with ActivityId. */
		virtual bool GetLeaveReplicationActivityById(const int64 ActivityId, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const = 0;

		virtual ~IReplicationWorkspace() = default;
	};
}
