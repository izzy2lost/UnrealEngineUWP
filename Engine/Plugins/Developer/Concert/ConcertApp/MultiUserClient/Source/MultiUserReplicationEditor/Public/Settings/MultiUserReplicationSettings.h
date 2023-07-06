// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MultiUserReplicationClientProfileAsset.h"
#include "Settings/DefaultPropertySelection.h"
#include "MultiUserReplicationSettings.generated.h"

/** Asset for users to describe a client to the server. */
UCLASS(Config=MultiUserClient)
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationSettings : public UObject
{
	GENERATED_BODY()
public:
	
	/** The client profile to use when connecting to a session. */
	UPROPERTY(EditAnywhere, Config, Category = "Replication")
	TSoftObjectPtr<UMultiUserReplicationClientProfileAsset> DefaultProfile;

	/** Properties you want selected by default when you add a new replicated object in the editor */
	UPROPERTY(EditAnywhere, Config, Category = "Replication|Editor")
	TMap<FSoftClassPath, FDefaultPropertySelection> DefaultPropertySelection;

	static UMultiUserReplicationSettings* Get() { return GetMutableDefault<UMultiUserReplicationSettings>(); }

	/** Reads DefaultPropertySelection and applies any default property selections to Info based on the Class just added. */
	void AddDefaultPropertiesFromSettings(FReplicatedObjectInfo& Info, UClass& Class);
};