// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Replication/Settings/ConcertStreamObjectAutoBindingRules.h"
#include "MultiUserReplicationSettings.generated.h"

UCLASS(config=MultiUserClient)
class UMultiUserReplicationSettings : public UObject
{
	GENERATED_BODY()
public:

	static UMultiUserReplicationSettings* Get() { return GetMutableDefault<UMultiUserReplicationSettings>(); }

	/**
	 * When you add an object via the stream editor, you may want to automatically bind properties and add additional subobjects.
	 * Here you can specify the rules to achieve this.
	 */
	UPROPERTY(Config, Category = "Replication Settings", EditAnywhere)
	FConcertStreamObjectAutoBindingRules ReplicationEditorSettings;
};