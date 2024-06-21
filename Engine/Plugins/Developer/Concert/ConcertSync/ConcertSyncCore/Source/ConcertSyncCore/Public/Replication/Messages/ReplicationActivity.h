// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertSyncSessionTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Replication/Data/ObjectIds.h"
#include "Replication/Data/ReplicationStream.h"
#include "ReplicationActivity.generated.h"

class FText;

/** Identifies the FConcertSyncReplicationEvent::Payload struct type */
UENUM()
enum class EConcertSyncReplicationActivityType : uint8
{
	None,
	
	/** Client left the replication session (either because they left the MU session or because they sent a FConcertReplication_LeaveEvent). */
	LeaveReplication,

	// ADD NEW ENTRIES ABOVE
	Count
};

/**
 * Contains the streams and authority a client had when they left a session.
 * Used for restoring content when the client rejoins.
 */
USTRUCT()
struct FConcertSyncReplicationPayload_LeaveReplication
{
	GENERATED_BODY()

	/** The streams the client had registered when they left. */
	UPROPERTY()
	TArray<FConcertReplicationStream> Streams;
	
	/** The objects the client had authority over when they left. */
	UPROPERTY()
	TArray<FConcertObjectInStreamID> OwnedObjects;

	friend bool operator==(const FConcertSyncReplicationPayload_LeaveReplication&, const FConcertSyncReplicationPayload_LeaveReplication&) = default;
	friend bool operator!=(const FConcertSyncReplicationPayload_LeaveReplication&, const FConcertSyncReplicationPayload_LeaveReplication&) = default;
};

USTRUCT()
struct FConcertSyncReplicationSummary_LeaveReplication
{
	GENERATED_BODY()

	/** The objects the client had authority over when they left. */
	UPROPERTY()
	TArray<FConcertObjectInStreamID> OwnedObjects;
};

namespace UE::ConcertSyncCore
{
	inline FString GetReplicationActivityPayloadTypePathName(EConcertSyncReplicationActivityType Type)
	{
		switch (Type)
		{
		case EConcertSyncReplicationActivityType::LeaveReplication: return FConcertSyncReplicationPayload_LeaveReplication::StaticStruct()->GetPathName();
		default: ensureMsgf(false, TEXT("Unknown replication activity type (%u)"), static_cast<uint8>(Type)); return TEXT("");
		}
	}
}

/** Data for a replication event in a Concert Sync Session */
USTRUCT()
struct FConcertSyncReplicationEvent
{
	GENERATED_BODY()

	/** Identifies the Payload struct type */
	UPROPERTY()
	EConcertSyncReplicationActivityType ActivityType = EConcertSyncReplicationActivityType::None;

	/**
	 * A FConcertSyncReplicationPayload_X type depending on ActivityType.
	 * 
	 * Serialized into the database using Cbor; do not change (for simplicity we always assume it is Cbor).
	 * If you change the serialization method, you'll get a failing check.
	 * If for whatever reason you MUST save it using a different method, adjust FConcertSyncSessionDatabaseStatements::AddReplicationData
	 * and FConcertSyncSessionDatabaseStatements::SetReplicationData.
	 */
	UPROPERTY()
	FConcertSessionSerializedPayload Payload{ EConcertPayloadSerializationMethod::Cbor };

	FConcertSyncReplicationEvent() = default;
	FConcertSyncReplicationEvent(const FConcertSyncReplicationPayload_LeaveReplication& Data)
	{
		SetPayload(Data);
	}

	void SetPayload(const FConcertSyncReplicationPayload_LeaveReplication& Data)
	{
		ActivityType = EConcertSyncReplicationActivityType::LeaveReplication;
		Payload.SetTypedPayload(Data);
	}
	
	bool GetPayload(FConcertSyncReplicationPayload_LeaveReplication& Result) const
	{
		check(ActivityType == EConcertSyncReplicationActivityType::LeaveReplication);
		return Payload.GetTypedPayload(Result);
	}

	CONCERTSYNCCORE_API friend bool operator==(const FConcertSyncReplicationEvent& Left, const FConcertSyncReplicationEvent& Right);
	friend bool operator!=(const FConcertSyncReplicationEvent&, const FConcertSyncReplicationEvent&) = default;
};

/** Data for a replication activity entry in a Concert Sync Session */
USTRUCT()
struct FConcertSyncReplicationActivity : public FConcertSyncActivity
{
	GENERATED_BODY()

	FConcertSyncReplicationActivity()
	{
		EventType = EConcertSyncActivityEventType::Replication;
	}

	FConcertSyncReplicationActivity(const FConcertSyncReplicationPayload_LeaveReplication& PayloadData)
		: EventData(PayloadData)
	{
		EventType = EConcertSyncActivityEventType::Replication;
	}

	/** The transaction event data associated with this activity */
	UPROPERTY()
	FConcertSyncReplicationEvent EventData;
};

/** Summary for a lock activity entry in a Concert Sync Session */
USTRUCT()
struct CONCERTSYNCCORE_API FConcertSyncReplicationActivitySummary : public FConcertSyncActivitySummary
{
	GENERATED_BODY()

	/** The type of replication event we summarize */
	UPROPERTY()
	EConcertSyncReplicationActivityType ActivityType;

	/** The summary data. The underlying type depends on ActivityType. */
	UPROPERTY()
	FConcertSessionSerializedPayload Payload{ EConcertPayloadSerializationMethod::Cbor };

	bool GetSummaryData(FConcertSyncReplicationSummary_LeaveReplication& Data) const
	{
		check(ActivityType == EConcertSyncReplicationActivityType::LeaveReplication);
		return Payload.GetTypedPayload(Data);
	}

	/** Create this summary from a replication event */
	static FConcertSyncReplicationActivitySummary CreateSummaryForEvent(const FConcertSyncReplicationEvent& InEvent);

protected:
	
	//~ Begin FConcertSyncActivitySummary Interface
	virtual FText CreateDisplayText(const bool InUseRichText) const override;
	virtual FText CreateDisplayTextForUser(const FText InUserDisplayName, const bool InUseRichText) const override;
	//~ End FConcertSyncActivitySummary Interface
};