// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectReplicationMap.h"
#include "ReplicationStreamDescription.generated.h"

template<typename OptionalType>
struct TOptional;

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
};

/**
 * Contains a description of a replication stream.
 * A replication stream is a map of objects to their replicated properties, and a set of stream attributes (for rules).
 */
USTRUCT()
struct FReplicationStreamDescription
{
	GENERATED_BODY()

	UPROPERTY()
	FSharedReplicationStreamDescription BaseDescription;

	// TODO: Add TArray<TObjectPtr<UReplicationStreamAttribute>> property

	/** Packs the UObjects in this description (Concert does not allow sending UObjects directly) */
	CONCERTSYNCCORE_API struct FReplicationStreamDescription_NetPacked Pack() const;
};

/**
 * This data is packed such that it can be sent via concert messages; packed here means that instanced UObjects are wrapped
 * with sufficient data to reconstruct them.
 */
USTRUCT()
struct FReplicationStreamDescription_NetPacked
{
	GENERATED_BODY()

	
	UPROPERTY()
	FSharedReplicationStreamDescription BaseDescription;

	// TODO: Add TArray<FConcertSessionSerializedPayload> where each corresponds to a UReplicationStreamAttribute; those will be used by receive rules to decide whether a client can receive certain data.

	/** Unpacks this data. This function can fail, e.g if the UObject class does not exist locally. */
	CONCERTSYNCCORE_API TOptional<FReplicationStreamDescription> Unpack(FString* OutErrorMessage = nullptr) const;
};