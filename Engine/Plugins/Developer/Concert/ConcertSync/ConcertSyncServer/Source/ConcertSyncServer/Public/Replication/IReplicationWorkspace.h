// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/Optional.h"
#include "Templates/FunctionFwd.h"

enum class EBreakBehavior : uint8;

struct FConcertSyncReplicationActivity;
struct FConcertSyncReplicationPayload_Mute;
struct FConcertSessionClientInfo;
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
		
		/**
		 * Creates a replication activity for the client leaving replication
		 * @return The identifier of the produced activity. Unset if activity insertion failed.
		 */
		virtual TOptional<int64> ProduceClientLeaveReplicationActivity(const FGuid& EndpointId, const FConcertSyncReplicationPayload_LeaveReplication& EventData) = 0;

		/**
		 * Creates a replication activity for the client (un)muting objects in the session.
		 * @return The identifier of the produced activity. Unset if activity insertion failed.
		 */
		virtual TOptional<int64> ProduceClientMuteReplicationActivity(const FGuid& EndpointId, const FConcertSyncReplicationPayload_Mute& EventData) = 0;

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
		virtual bool GetLastLeaveReplicationActivityByClient(const FConcertSessionClientInfo& InClientInfo, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const = 0;
		/** Gets the replication leave activity with ActivityId. */
		virtual bool GetLeaveReplicationActivityById(const int64 ActivityId, FConcertSyncReplicationPayload_LeaveReplication& OutLeaveReplication) const = 0;

		/** Enumerates all mute activities. */
		virtual void EnumerateMuteActivities(TFunctionRef<EBreakBehavior(const FConcertSyncReplicationActivity& Activity)> Callback) const = 0;

		virtual ~IReplicationWorkspace() = default;
	};
}
