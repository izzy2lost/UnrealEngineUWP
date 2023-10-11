// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/MultiUserReplicationClientPreset.h"

#include "Assets/MultiUserReplicationStream.h"
#include "Replication/Data/ReplicationStreamDescription.h"

UMultiUserReplicationClientPreset::UMultiUserReplicationClientPreset()
{
	Stream = CreateDefaultSubobject<UMultiUserReplicationStream>(TEXT("ReplicationList"));
	Stream->StreamId = MultiUserStreamID;
	Stream->SetFlags(RF_Transactional);
}

void UMultiUserReplicationClientPreset::ClearClient()
{
	Stream->ReplicationMap.ReplicatedObjects.Empty();
}

FReplicationStreamDescription UMultiUserReplicationClientPreset::GenerateDescription() const
{
	return Stream->GenerateDescription();
}
