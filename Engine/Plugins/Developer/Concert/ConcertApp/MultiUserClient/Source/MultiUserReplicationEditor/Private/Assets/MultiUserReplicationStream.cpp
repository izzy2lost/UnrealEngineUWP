// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/MultiUserReplicationStream.h"

#include "Replication/Data/ReplicationStream.h"

UMultiUserReplicationStream::UMultiUserReplicationStream()
{
	// To avoid issues with delta serialization the CDO is always 0
	StreamId = HasAnyFlags(RF_ClassDefaultObject)
		? FGuid{}
		: FGuid::NewGuid();
}

FConcertReplicationStream UMultiUserReplicationStream::GenerateDescription() const
{
	return { StreamId, ReplicationMap, FrequencySettings };
}

void UMultiUserReplicationStream::Copy(UMultiUserReplicationStream& OtherStream)
{
	StreamId = OtherStream.StreamId;
	ReplicationMap = OtherStream.ReplicationMap;
	FrequencySettings = OtherStream.FrequencySettings;
}
