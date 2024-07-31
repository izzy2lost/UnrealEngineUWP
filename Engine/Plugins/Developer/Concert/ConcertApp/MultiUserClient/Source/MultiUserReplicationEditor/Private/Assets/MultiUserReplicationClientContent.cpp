// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/MultiUserReplicationClientContent.h"

#include "Assets/MultiUserReplicationStream.h"
#include "Replication/Data/ReplicationStream.h"

UMultiUserReplicationClientContent::UMultiUserReplicationClientContent()
{
	Stream = CreateDefaultSubobject<UMultiUserReplicationStream>(TEXT("ReplicationList"));
	Stream->SetFlags(RF_Transactional);
}

FConcertReplicationStream UMultiUserReplicationClientContent::GenerateDescription() const
{
	return Stream->GenerateDescription();
}
