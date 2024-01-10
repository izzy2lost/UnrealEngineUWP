// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectReplicationMap.h"
#include "ReplicationFrequencySettings.h"
#include "ReplicationStreamDescription.generated.h"

template<typename OptionalType>
struct TOptional;

namespace UE::ConcertSyncCore
{
	enum class EReplicationStreamCloneFlags : uint8
	{
		None,
		SkipProperties = 1 << 0,
		SkipFrequency = 1 << 1
	};
	ENUM_CLASS_FLAGS(EReplicationStreamCloneFlags)
}

/** Shared non-UObject data, which can be serialized by Concert without wrapping (UObjects need wrapping). */
USTRUCT()
struct FSharedReplicationStreamDescription
{
	GENERATED_BODY()
	
	/** Unique id for this stream. */
	UPROPERTY()
	FGuid Identifier;

	/** Identifies the data that this stream will send. */
	UPROPERTY()
	FObjectReplicationMap ReplicationMap;

	/**
	 * Determines how often updates are sent for objects.
	 *
	 * The purposes of registering frequency with the server communicates the sending client's intent and has these advantages:
	 * - If the server receives replicated data more often than specified here, it will combine and throttle the events forwarded to the other clients.
	 * - The receiving clients can query how often per frame to expect updates for an object
	 *
	 * On a technical level, this field is not required for replicating objects: in a hypothetical, alternate implementation, the sending client could
	 * just keep the replication frequency local to themselves and send at the rate it wants to send.
	 * This field is here mostly to allow inspection by other parties, which does add overhead (engineering and runtime) because we must support
	 * FConcertReplication_ChangeStream_Frequency requests).
	 * 
	 * With that said, the sending implementation uses the rate that is specified here to throttle the rate at which it sends update to the server.
	 * @see FObjectReplicationProcessor::ProcessObjects.
	 */
	UPROPERTY()
	FConcertStreamFrequencySettings FrequencySettings;

	/** Util for efficiently cloning stream info and optionally skipping some info. */
	FSharedReplicationStreamDescription Clone(UE::ConcertSyncCore::EReplicationStreamCloneFlags Flags = UE::ConcertSyncCore::EReplicationStreamCloneFlags::None) const;
	
	friend bool operator==(const FSharedReplicationStreamDescription& Left, const FSharedReplicationStreamDescription& Right)
	{
		return Left.Identifier == Right.Identifier
			&& Left.ReplicationMap == Right.ReplicationMap
			&& Left.FrequencySettings == Right.FrequencySettings;
	}
	friend bool operator!=(const FSharedReplicationStreamDescription& Left, const FSharedReplicationStreamDescription& Right)
	{
		return !(Left == Right);
	}
};

/**
 * Contains a description of a replication stream.
 * A replication stream
 * - is a map of objects to their replicated properties,
 * - a set of stream attributes (for rules, TODO UE-190167)
 */
USTRUCT()
struct FReplicationStreamDescription
{
	GENERATED_BODY()

	UPROPERTY()
	FSharedReplicationStreamDescription BaseDescription;

	friend bool operator==(const FReplicationStreamDescription& Left, const FReplicationStreamDescription& Right)
	{
		return Left.BaseDescription == Right.BaseDescription;
	}
	friend bool operator!=(const FReplicationStreamDescription& Left, const FReplicationStreamDescription& Right)
	{
		return !(Left == Right);
	}

	/** Packs the UObjects in this description (Concert does not allow sending UObjects directly) */
	CONCERTSYNCCORE_API struct FReplicationStreamDescription_NetPacked Pack() const;
};

inline FSharedReplicationStreamDescription FSharedReplicationStreamDescription::Clone(UE::ConcertSyncCore::EReplicationStreamCloneFlags Flags) const
{
	using namespace UE::ConcertSyncCore;
	FSharedReplicationStreamDescription Result { Identifier };

	const TMap<FSoftObjectPath, FReplicatedObjectInfo>& ReplicatedObjects = ReplicationMap.ReplicatedObjects;
	if (EnumHasAnyFlags(Flags, EReplicationStreamCloneFlags::SkipProperties)
		&& !ReplicationMap.IsEmpty())
	{
		Result.ReplicationMap.ReplicatedObjects.Reserve(ReplicatedObjects.Num());
		for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& ObjectInfo : ReplicatedObjects)
		{
			Result.ReplicationMap.ReplicatedObjects.Add(ObjectInfo.Key, { ObjectInfo.Value.ClassPath });
		}
	}
	else
	{
		Result.ReplicationMap = ReplicationMap;
	}

	if (!EnumHasAnyFlags(Flags, EReplicationStreamCloneFlags::SkipFrequency))
	{
		Result.FrequencySettings = FrequencySettings;
	}
		
	return Result;
}