// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class UMultiUserReplicationClientProfileAsset;
class IConcertSyncClient;

namespace UE::MultiUserClient
{
	/**
	 * Interacts with the replication system on behalf of Multi-User to execute actions specific to Multi-User workflows;
	 * this is opposed to other uses of the replication API, e.g. users using the system in a shipped game.
	 *
	 * This class will
	 *  - be used as a model for the MU control views, such as displaying the streams in the current session (TODO DP UE-193541).
	 *  - implement auto-join behavior in response to joining a concert session (TODO DP UE-193538)
	 *
	 * This class implements the Fence design pattern. All knowledge Multi-User might need should be encapsulated by this class.
	 */
	class FMultiUserReplicationManager
	{
	public:

		FMultiUserReplicationManager(TSharedRef<IConcertSyncClient> InClient);

		/** Joins a replication session. */
		void JoinReplicationSession(const UMultiUserReplicationClientProfileAsset& Asset);
		/** Leaves the current replication session */
		void LeaveSession();

		/** Whether it is valid to call JoinReplicationSession. */
		bool CanJoin() const;
		/** Whether it is valid to call LeaveSession. */
		bool IsConnectedToReplicationSession() const;

	private:

		/** Client through which the replication bridge is accessed. */
		TSharedRef<IConcertSyncClient> Client;
	};
}

