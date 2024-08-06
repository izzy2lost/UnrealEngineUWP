// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Misc/StreamAndAuthorityPredictionUtils.h"

#include "ConcertMessageData.h"

namespace UE::ConcertSyncCore::Replication
{
	bool AreLogicallySameClients(const FConcertClientInfo& First, const FConcertClientInfo& Second)
	{
		return First.DisplayName == Second.DisplayName && First.DeviceName == Second.DeviceName;
	}
}
