// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Messages/ReplicationActivity.h"

#include "Internationalization/Text.h"

bool operator==(const FConcertSyncReplicationEvent& Left, const FConcertSyncReplicationEvent& Right)
{
	if (Left.ActivityType != Right.ActivityType)
	{
		return false;
	}

	static_assert(static_cast<int32>(EConcertSyncReplicationActivityType::Count) == 2, "If you added an EConcertSyncReplicationActivityType entry, update this switch");
	switch (Left.ActivityType)
	{
	case EConcertSyncReplicationActivityType::None: return true;
	case EConcertSyncReplicationActivityType::LeaveReplication:
		{
			FConcertSyncReplicationPayload_LeaveReplication LeftContent;
			FConcertSyncReplicationPayload_LeaveReplication RightContent;
			const bool bLeftSucceeded = Left.GetPayload(LeftContent);
			const bool bRightSucceeded = Right.GetPayload(RightContent);
			return bLeftSucceeded && bRightSucceeded && LeftContent == RightContent;
		}
	default: checkNoEntry(); return false;
	}
}

FConcertSyncReplicationActivitySummary FConcertSyncReplicationActivitySummary::CreateSummaryForEvent(const FConcertSyncReplicationEvent& InEvent)
{
	FConcertSyncReplicationActivitySummary Summary;
	Summary.ActivityType = InEvent.ActivityType;

	static_assert(static_cast<uint8>(EConcertSyncReplicationActivityType::Count) == 2, "If you changed the enum entries, update this switch");
	switch (Summary.ActivityType)
	{
	case EConcertSyncReplicationActivityType::LeaveReplication:
		{
			FConcertSyncReplicationPayload_LeaveReplication LeaveReplicationData;
			InEvent.GetPayload(LeaveReplicationData);
			
			const FConcertSyncReplicationSummary_LeaveReplication Content { LeaveReplicationData.OwnedObjects };
			Summary.Payload.SetTypedPayload(Content);
		}
		break;
	case EConcertSyncReplicationActivityType::None: [[fallthrough]];
	default: checkNoEntry(); break;
	}
	
	return Summary;
}