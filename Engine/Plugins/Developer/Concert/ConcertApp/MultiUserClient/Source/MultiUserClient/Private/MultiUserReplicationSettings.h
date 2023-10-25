// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Replication/Settings/ConcertReplicationEditorSettings.h"
#include "MultiUserReplicationSettings.generated.h"

UCLASS(config=MultiUserClient)
class UMultiUserReplicationSettings : public UObject
{
	GENERATED_BODY()
public:

	static UMultiUserReplicationSettings* Get() { return GetMutableDefault<UMultiUserReplicationSettings>(); }

	/** Settings that affect the replication editor */
	UPROPERTY(Config, Category = "Replication Settings", EditAnywhere)
	FConcertReplicationEditorSettings ReplicationEditorSettings;
};