// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationStreamAsset.h"

UMultiUserReplicationStreamAsset::UMultiUserReplicationStreamAsset()
{
	ReplicationList = CreateDefaultSubobject<UMultiUserPropertyReplicationSelection>(TEXT("ReplicationList"));
	ReplicationList->SetFlags(RF_Transactional);

	// To avoid issues with delta serialization the CDO is always 0
	StreamId = HasAnyFlags(RF_ClassDefaultObject)
		? FGuid{}
		: FGuid::NewGuid();
}

FReplicationStreamDescription UMultiUserReplicationStreamAsset::GenerateDescription() const
{
	return { StreamId, ReplicationList->ReplicationMap };
}
