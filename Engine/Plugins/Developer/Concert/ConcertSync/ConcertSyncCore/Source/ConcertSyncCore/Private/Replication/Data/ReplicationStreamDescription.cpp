// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Data/ReplicationStreamDescription.h"

#include "Misc/Optional.h"

FReplicationStreamDescription_NetPacked FReplicationStreamDescription::Pack() const
{
	// TODO: Implement once stream attributes are added
	return FReplicationStreamDescription_NetPacked{ BaseDescription };
}

TOptional<FReplicationStreamDescription> FReplicationStreamDescription_NetPacked::Unpack(FString* OutErrorMessage) const
{
	// TODO: Implement once stream attributes are added
	return FReplicationStreamDescription{ BaseDescription };
}

