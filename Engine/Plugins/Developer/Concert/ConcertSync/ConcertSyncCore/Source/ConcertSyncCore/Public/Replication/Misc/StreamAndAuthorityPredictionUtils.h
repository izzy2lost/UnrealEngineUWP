// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"

namespace UE::ConcertSyncCore::Replication
{
	/**
	 * Decides whether First and Second should be considered to represent the same user across several Concert sessions.
	 *
	 * Every time a user joins a Concert session, a new endpoint ID is generated for that user and saved in the database.
	 * Even though the endpoint ID is different, we can associate the same user across the IDs by using the DisplayName and DeviceName.
	 *
	 * @return Whether First and Second logically describe the same client (i.e. DisplayName and DeviceName of both clients are equal).
	 */
	CONCERTSYNCCORE_API bool AreLogicallySameClients(const FConcertClientInfo& First, const FConcertClientInfo& Second);
}
