// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationStreamAsset.h"

UMultiUserReplicationStreamAsset::UMultiUserReplicationStreamAsset()
{
	ReplicationList = CreateDefaultSubobject<UMultiUserPropertyReplicationSelection>(TEXT("ReplicationList"));
	ReplicationList->SetFlags(RF_Transactional);
}

FReplicationStreamDescription UMultiUserReplicationStreamAsset::GenerateDescription() const
{
	// TODO:
	return {};
}
