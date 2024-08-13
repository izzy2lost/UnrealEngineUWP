// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Replication/Data/ReplicationStream.h"
#include "MultiUserReplicationClientPreset.generated.h"

/** Stores info about a client's content in a preset. */
USTRUCT()
struct FMultiUserReplicationClientPreset
{
	GENERATED_BODY()

	/** The objects this stream will modify. */
	UPROPERTY()
	FConcertObjectReplicationMap ReplicationMap;
	
	/** The frequency setting the stream has. */
	UPROPERTY()
	FConcertStreamFrequencySettings FrequencySettings;

	/** The FConcertClientInfo::DisplayName of the client. */
	UPROPERTY()
	FString DisplayName;
	/** The FConcertClientInfo::DeviceName of the client. */
	UPROPERTY()
	FString DeviceName;

	FMultiUserReplicationClientPreset() = default;

	FMultiUserReplicationClientPreset(const FString& DisplayName, const FString& DeviceName)
		: DisplayName(DisplayName)
		, DeviceName(DeviceName)
	{}
};
