// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MultiUserReplicationStream.h"
#include "Replication/Data/ReplicationStream.h"
#include "MultiUserReplicationClientContent.generated.h"

/** The transactable content of a FReplicationClient. */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationClientContent : public UObject
{
	GENERATED_BODY()
public:

	/** The stream this client is managing */
	UPROPERTY(Instanced)
	TObjectPtr<UMultiUserReplicationStream> Stream;

	/** The FConcertClientInfo::DisplayName of the client. */
	UPROPERTY()
	FString DisplayName;
	/** The FConcertClientInfo::DeviceName of the client. */
	UPROPERTY()
	FString DeviceName;

	UMultiUserReplicationClientContent();

	/** Generates a description that can be sent to the MU server. */
	FConcertReplicationStream GenerateDescription() const;
};
