// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Replication/Data/ReplicationClientDescription.h"
#include "MultiUserReplicationStreamAsset.h"
#include "MultiUserReplicationClientProfileAsset.generated.h"

/** Asset for users to describe a client to the server. */
UCLASS()
class MULTIUSERREPLICATION_API UMultiUserReplicationClientProfileAsset : public UObject
{
	GENERATED_BODY()
public:

	/** Describes this client to the server */
	UPROPERTY(EditAnywhere, Category = "Replication")
	FReplicationClientDescription ClientDescription;

	/** The streams this client will send to the server. It takes authority over the listed properties. */
	UPROPERTY(EditAnywhere, Category = "Replication")
	TArray<TObjectPtr<UMultiUserReplicationStreamAsset>> Streams;
};