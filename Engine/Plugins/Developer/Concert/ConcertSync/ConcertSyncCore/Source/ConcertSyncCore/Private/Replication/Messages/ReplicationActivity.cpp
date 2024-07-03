// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Messages/ReplicationActivity.h"

#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "ReplicationActivity"

bool operator==(const FConcertSyncReplicationEvent& Left, const FConcertSyncReplicationEvent& Right)
{
	if (Left.ActivityType != Right.ActivityType)
	{
		return false;
	}

	static_assert(static_cast<int32>(EConcertSyncReplicationActivityType::Count) == 3, "If you added an EConcertSyncReplicationActivityType entry, update this switch");
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
	case EConcertSyncReplicationActivityType::Mute:
		{
			FConcertSyncReplicationPayload_Mute LeftContent;
			FConcertSyncReplicationPayload_Mute RightContent;
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

	static_assert(static_cast<uint8>(EConcertSyncReplicationActivityType::Count) == 3, "If you changed the enum entries, update this switch");
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
	case EConcertSyncReplicationActivityType::Mute:
		{
			FConcertSyncReplicationPayload_Mute MuteData;
			InEvent.GetPayload(MuteData);
			
			const FConcertSyncReplicationSummary_Mute Content { MuteData.Request };
			Summary.Payload.SetTypedPayload(Content);
		}
		break;
	case EConcertSyncReplicationActivityType::None: [[fallthrough]];
	default: checkNoEntry(); break;
	}
	
	return Summary;
}

FText FConcertSyncReplicationActivitySummary::ToDisplayTitle() const
{
	static_assert(static_cast<uint8>(EConcertSyncReplicationActivityType::Count) == 3, "If you changed the enum entries, update this switch");
	switch (ActivityType)
	{
	case EConcertSyncReplicationActivityType::LeaveReplication: return LOCTEXT("Title.LeftReplication", "Left Replication");
	case EConcertSyncReplicationActivityType::Mute:
		{
			FConcertSyncReplicationSummary_Mute SummaryData;
			if (!GetSummaryData(SummaryData))
			{
				return LOCTEXT("Title.Mute.FailedToGetData", "Pause / Resume");
			}

			const bool bHasMuted = !SummaryData.Request.ObjectsToMute.IsEmpty();
			const bool bHasUnmuted = !SummaryData.Request.ObjectsToUnmute.IsEmpty();
			if (bHasMuted && bHasUnmuted)
			{
				return LOCTEXT("Title.Mute.PauseAndResume", "Pause & Resume");
			}

			if (bHasMuted)
			{
				return LOCTEXT("Title.Mute.Pause", "Pause replication");
			}
			
			if (bHasUnmuted)
			{
				return LOCTEXT("Title.Mute.Resume", "Resume replication");
			}

			return LOCTEXT("Title.Mute.Resume", "Pause / Resume (empty)");
		}
	
	case EConcertSyncReplicationActivityType::None: [[fallthrough]];
	default: checkNoEntry(); return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE