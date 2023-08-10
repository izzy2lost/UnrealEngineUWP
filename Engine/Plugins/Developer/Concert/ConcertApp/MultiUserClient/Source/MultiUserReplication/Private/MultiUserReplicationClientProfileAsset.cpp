// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationClientProfileAsset.h"

#include "Replication/IConcertClientReplicationManager.h"

#include "Algo/Transform.h"

UE::ConcertSyncClient::Replication::FJoinReplicatedSessionArgs UMultiUserReplicationClientProfileAsset::ToJoinArgs() const
{
	UE::ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Result;
	Result.ClientInfo = ClientDescription;
	Algo::TransformIf(Streams, Result.Streams,
		[](const UMultiUserReplicationStreamAsset* StreamAsset)
		{
			return StreamAsset != nullptr;
		},
		[](const UMultiUserReplicationStreamAsset* StreamAsset)
		{
			return StreamAsset->GenerateDescription();
		});
	return Result;
}
